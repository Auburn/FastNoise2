#!/usr/bin/env python3
"""Runs FastNoise2 CodeGen shader output on a GPU and compares it with the reference evaluation.

    FastNoiseCodeGenTest --export cases.jsonl
    python3 CodeGenGpuCheck.py cases.jsonl [--languages glsl,wgsl,hlsl]

Requires: pip install numpy moderngl wgpu, and glslangValidator on PATH for HLSL.
Works headless with Mesa (llvmpipe for OpenGL, lavapipe for Vulkan/WebGPU).
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile

import numpy as np

WORKGROUP = 64
COMPONENTS = "xyzw"


def position_args(dimensions, index, stride, suffix=""):
    return ", ".join(f"gPositions[{index} * {stride}{suffix} + {d}{suffix}]" for d in range(dimensions))


class GlslRunner:
    def __init__(self):
        import moderngl
        try:
            self.ctx = moderngl.create_standalone_context(backend="egl", require=430)
        except Exception:
            self.ctx = moderngl.create_standalone_context(require=430)
        self.name = "GLSL (" + self.ctx.info["GL_RENDERER"] + ")"

    def run(self, case, positions):
        dims = case["dimensions"]
        source = "\n".join([
            "#version 430",
            f"layout( local_size_x = {WORKGROUP} ) in;",
            "layout( std430, binding = 0 ) readonly buffer Positions { float gPositions[]; };",
            "layout( std430, binding = 1 ) writeonly buffer Output { float gOutput[]; };",
            "uniform int uSeed;",
            "uniform uint uCount;",
            case["glsl"],
            "void main()",
            "{",
            "    uint i = gl_GlobalInvocationID.x;",
            "    if( i >= uCount ) return;",
            f"    gOutput[i] = fastnoise( vec{dims}( {position_args( dims, 'i', dims, 'u' )} ), uSeed );",
            "}",
        ])
        count = len(positions) // dims
        shader = self.ctx.compute_shader(source)
        position_buffer = self.ctx.buffer(positions.tobytes())
        output_buffer = self.ctx.buffer(reserve=count * 4)
        position_buffer.bind_to_storage_buffer(0)
        output_buffer.bind_to_storage_buffer(1)
        # Uniforms unused by the generated code are optimised out
        for uniform, value in (("uSeed", case["seed"]), ("uCount", count)):
            if uniform in shader:
                shader[uniform] = value
        shader.run(group_x=(count + WORKGROUP - 1) // WORKGROUP)
        result = np.frombuffer(output_buffer.read(), dtype=np.float32).copy()
        for resource in (shader, position_buffer, output_buffer):
            resource.release()
        return result


class WgpuRunner:
    def __init__(self, language):
        import wgpu
        self.wgpu = wgpu
        self.language = language
        adapter = wgpu.gpu.request_adapter_sync(power_preference="high-performance")
        self.device = adapter.request_device_sync()
        self.name = f"{language.upper()} ({adapter.info['device']}, {adapter.info['backend_type']})"

    def wgsl_module(self, case):
        dims = case["dimensions"]
        return "\n".join([
            "@group( 0 ) @binding( 0 ) var<storage, read> gPositions: array<f32>;",
            "@group( 0 ) @binding( 1 ) var<storage, read_write> gOutput: array<f32>;",
            "struct Params { seed: i32, count: u32 }",
            "@group( 0 ) @binding( 2 ) var<uniform> gParams: Params;",
            case["wgsl"],
            f"@compute @workgroup_size( {WORKGROUP} )",
            "fn main( @builtin( global_invocation_id ) id: vec3<u32> )",
            "{",
            "    let i = id.x;",
            "    if( i >= gParams.count ) { return; }",
            f"    gOutput[i] = fastnoise( vec{dims}<f32>( {position_args( dims, 'i', dims, 'u' )} ), gParams.seed );",
            "}",
        ])

    def hlsl_spirv(self, case):
        dims = case["dimensions"]
        source = "\n".join([
            "[[vk::binding( 0, 0 )]] StructuredBuffer<float> gPositions;",
            "[[vk::binding( 1, 0 )]] RWStructuredBuffer<float> gOutput;",
            "[[vk::binding( 2, 0 )]] cbuffer Params { int gSeed; uint gCount; };",
            case["hlsl"],
            f"[numthreads( {WORKGROUP}, 1, 1 )]",
            "void main( uint3 id : SV_DispatchThreadID )",
            "{",
            "    uint i = id.x;",
            "    if( i >= gCount ) return;",
            f"    gOutput[i] = fastnoise( float{dims}( {position_args( dims, 'i', dims, 'u' )} ), gSeed );",
            "}",
        ])
        with tempfile.TemporaryDirectory() as tmp:
            hlsl_path = os.path.join(tmp, "shader.comp.hlsl")
            spirv_path = os.path.join(tmp, "shader.spv")
            with open(hlsl_path, "w") as f:
                f.write(source)
            result = subprocess.run(["glslangValidator", "-D", "-V", "-S", "comp", "-e", "main", "-o", spirv_path, hlsl_path],
                                    capture_output=True, text=True)
            if result.returncode != 0:
                raise RuntimeError(result.stdout + result.stderr)
            with open(spirv_path, "rb") as f:
                return f.read()

    def run(self, case, positions):
        wgpu = self.wgpu
        code = self.wgsl_module(case) if self.language == "wgsl" else self.hlsl_spirv(case)
        count = len(positions) // case["dimensions"]

        shader = self.device.create_shader_module(code=code)
        # Explicit layout, "auto" drops bindings the shader compiler eliminated (eg. positions for a Constant node)
        storage = wgpu.BufferBindingType.read_only_storage
        bind_group_layout = self.device.create_bind_group_layout(entries=[
            {"binding": 0, "visibility": wgpu.ShaderStage.COMPUTE, "buffer": {"type": storage}},
            {"binding": 1, "visibility": wgpu.ShaderStage.COMPUTE, "buffer": {"type": wgpu.BufferBindingType.storage}},
            {"binding": 2, "visibility": wgpu.ShaderStage.COMPUTE, "buffer": {"type": wgpu.BufferBindingType.uniform}},
        ])
        pipeline_layout = self.device.create_pipeline_layout(bind_group_layouts=[bind_group_layout])
        pipeline = self.device.create_compute_pipeline(layout=pipeline_layout, compute={"module": shader, "entry_point": "main"})

        position_buffer = self.device.create_buffer_with_data(data=positions.tobytes(), usage=wgpu.BufferUsage.STORAGE)
        output_buffer = self.device.create_buffer(size=count * 4, usage=wgpu.BufferUsage.STORAGE | wgpu.BufferUsage.COPY_SRC)
        params = np.array([case["seed"], count, 0, 0], dtype=np.int32)
        params_buffer = self.device.create_buffer_with_data(data=params.tobytes(), usage=wgpu.BufferUsage.UNIFORM)

        bind_group = self.device.create_bind_group(layout=bind_group_layout, entries=[
            {"binding": 0, "resource": {"buffer": position_buffer}},
            {"binding": 1, "resource": {"buffer": output_buffer}},
            {"binding": 2, "resource": {"buffer": params_buffer}},
        ])

        encoder = self.device.create_command_encoder()
        compute = encoder.begin_compute_pass()
        compute.set_pipeline(pipeline)
        compute.set_bind_group(0, bind_group)
        compute.dispatch_workgroups((count + WORKGROUP - 1) // WORKGROUP)
        compute.end()
        self.device.queue.submit([encoder.finish()])

        return np.frombuffer(self.device.queue.read_buffer(output_buffer), dtype=np.float32).copy()


def to_array(values):
    return np.array([np.nan if v is None else v for v in values], dtype=np.float32)


def compare(gpu, reference):
    finite = reference[np.isfinite(reference)]
    scale = max(1e-6, float(np.max(np.abs(finite)))) if len(finite) else 1.0
    with np.errstate(invalid="ignore"):
        error = np.abs(gpu - reference)
        same_nan = np.isnan(gpu) & np.isnan(reference)
        close = same_nan | (error <= 1e-3 * scale) | (~np.isfinite(reference))
        max_error = float(np.max(np.where(np.isfinite(error), error, 0)))
    return float(np.mean(close)), max_error / scale


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cases")
    parser.add_argument("--languages", default="glsl,wgsl,hlsl")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    with open(args.cases) as f:
        cases = [json.loads(line) for line in f]

    failures = 0
    skipped = 0
    for language in args.languages.split(","):
        # Created one at a time, initialising a Vulkan device leaves the EGL context of an earlier runner unusable
        runner = GlslRunner() if language == "glsl" else WgpuRunner(language)
        closeness = []
        worst = []
        for case in cases:
            positions = np.array(case["positions"], dtype=np.float32)
            reference = to_array(case["reference"])
            cpu = to_array(case["cpu"])

            # Trees with a constant NaN/inf output (eg. Divide 0 / 0): glslang folds the HLSL bitcast to a non finite
            # constant which naga rejects, D3D compilers accept it
            if not np.any(np.isfinite(reference)) and language == "hlsl":
                skipped += 1
                continue

            try:
                gpu = runner.run(case, positions)
            except Exception as e:
                failures += 1
                print(f"FAIL {runner.name} {case['name']} {case['dimensions']}D: {str( e )[:2000]}")
                continue

            close, max_error = compare(gpu, reference)
            # Chaotic trees (hashing float positions) are as far from the CPU reference as the relaxed CPU is
            cpu_close, _ = compare(cpu, reference)
            closeness.append(close)
            worst.append((close, case["name"], case["dimensions"], max_error, cpu_close))

            if close < min(0.98, cpu_close * 0.9 - 0.01) and cpu_close >= 0.5:
                failures += 1
                print(f"FAIL {runner.name} {case['name']} {case['dimensions']}D: close {close:.3f} (relaxed cpu {cpu_close:.3f}) max rel error {max_error:.3g} {case['encoded']}")
            elif args.verbose:
                print(f"ok   {runner.name} {case['name']} {case['dimensions']}D: close {close:.3f} max rel error {max_error:.3g}")

        if closeness:
            worst.sort()
            print(f"{runner.name}: {len( closeness )} cases, mean close {np.mean( closeness ):.5f}, "
                  f"{sum( c == 1.0 for c in closeness )} with every sample close")
            for close, name, dims, max_error, cpu_close in worst[:5]:
                print(f"    lowest: {name} {dims}D close {close:.3f} (relaxed cpu {cpu_close:.3f}) max rel error {max_error:.3g}")

    if skipped:
        print(f"{skipped} HLSL cases with no finite output skipped")
    print("FAILED" if failures else "PASSED", failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
