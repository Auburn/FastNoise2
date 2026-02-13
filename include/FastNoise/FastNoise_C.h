#ifndef FASTNOISE_C_H
#define FASTNOISE_C_H

/** @file FastNoise_C.h
 *  @brief C API for FastNoise2 noise generation.
 *
 *  Provides a plain-C interface to the FastNoise2 library, suitable for use from
 *  C code, FFI bindings, and other languages that cannot consume C++ headers directly.
 *  Nodes are represented as opaque `void*` handles and must be freed with fnDeleteNodeRef().
 */

#include "Utility/Export.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Create a node tree from an encoded string.
 *
 *  Equivalent to FastNoise::NewFromEncodedNodeTree() in the C++ API.
 *  The returned handle must be freed with fnDeleteNodeRef() when no longer needed.
 *
 *  @param encodedString  Encoded node tree string (e.g. from the Node Editor).
 *  @param maxFeatureSet  Maximum auto-detected SIMD feature set. Pass ~0u (uint32_max) for no limit.
 *  @return Opaque node handle, or NULL on failure.
 */
FASTNOISE_API void* fnNewFromEncodedNodeTree( const char* encodedString, unsigned /*FastSIMD::FeatureSet*/ maxFeatureSet );

/** @brief Release a node handle previously obtained from fnNewFromEncodedNodeTree() or fnNewFromMetadata().
 *  @param node  Node handle to release. May be NULL (no-op).
 */
FASTNOISE_API void fnDeleteNodeRef( void* node );

/** @brief Get the SIMD feature set level active for this node.
 *  @param node  Node handle.
 *  @return FastSIMD::FeatureSet value as a uint.
 */
FASTNOISE_API unsigned fnGetActiveFeatureSet( const void* node );

/** @brief Get the metadata ID for this node's type.
 *  @param node  Node handle.
 *  @return Metadata node_id that can be used with fnGetMetadataName() etc.
 */
FASTNOISE_API int fnGetMetadataID( const void* node );

/** @brief Generate a 2D uniform grid of noise values.
 *
 *  Fills @p noiseOut with noise sampled on a regular 2D grid. Ideal for
 *  generating noise for 2D textures, heightmaps, or image buffers that
 *  require uniform spacing between sample positions.
 *
 *  Output values are written in row-major order with X as the inner loop:
 *  `noiseOut[y * xCount + x]`
 *
 *  @param node         Node handle (must be the root of the node tree).
 *  @param noiseOut     Pre-allocated output array. Must hold at least `xCount * yCount` floats.
 *  @param xOffset      Starting X position in world space.
 *  @param yOffset      Starting Y position in world space.
 *  @param xCount       Number of samples along the X axis.
 *  @param yCount       Number of samples along the Y axis.
 *  @param xStepSize    Distance between samples along X.
 *  @param yStepSize    Distance between samples along Y.
 *  @param seed         Seed value for the noise. Different seeds produce different patterns.
 *  @param outputMinMax Optional float[2] to receive {min, max} of generated values. Pass NULL to skip.
 */
FASTNOISE_API void fnGenUniformGrid2D( const void* node, float* noiseOut,
                                       float xOffset, float yOffset,
                                       int xCount, int yCount,
                                       float xStepSize, float yStepSize,
                                       int seed, float* outputMinMax /*nullptr or float[2]*/ );

/** @brief Generate a 3D uniform grid of noise values.
 *
 *  Fills @p noiseOut with noise sampled on a regular 3D grid. Ideal for volumetric
 *  data such as voxel terrain, 3D textures, or density fields that
 *  require uniform spacing between sample positions.
 *
 *  Try to avoid setting xCount = 1 if you want a slice of 3D noise, due to how
 *  the positions are generated a small xCount is bad for performance.
 *  Use y/zCount = 1 instead.
 *
 *  Output values are written with X as the innermost loop, then Y, then Z:
 *  `noiseOut[(z * yCount + y) * xCount + x]`
 *
 *  @param node         Node handle (must be the root of the node tree).
 *  @param noiseOut     Pre-allocated output array. Must hold at least `xCount * yCount * zCount` floats.
 *  @param xOffset      Starting X position in world space.
 *  @param yOffset      Starting Y position in world space.
 *  @param zOffset      Starting Z position in world space.
 *  @param xCount       Number of samples along the X axis.
 *  @param yCount       Number of samples along the Y axis.
 *  @param zCount       Number of samples along the Z axis.
 *  @param xStepSize    Distance between samples along X.
 *  @param yStepSize    Distance between samples along Y.
 *  @param zStepSize    Distance between samples along Z.
 *  @param seed         Seed value for the noise. Different seeds produce different patterns.
 *  @param outputMinMax Optional float[2] to receive {min, max} of generated values. Pass NULL to skip.
 */
FASTNOISE_API void fnGenUniformGrid3D( const void* node, float* noiseOut,
                                       float xOffset, float yOffset, float zOffset,
                                       int xCount, int yCount, int zCount,
                                       float xStepSize, float yStepSize, float zStepSize,
                                       int seed, float* outputMinMax /*nullptr or float[2]*/ );

/** @brief Generate a 4D uniform grid of noise values.
 *
 *  Fills @p noiseOut with noise sampled on a regular 4D grid. The fourth dimension can be
 *  used for animating 3D noise over time or other parametric effects.
 *
 *  Try to avoid setting xCount = 1 if you want a slice of 4D noise, due to how
 *  the positions are generated a small xCount is bad for performance.
 *  Use y/z/wCount = 1 instead.
 *
 *  Output values are written with X as the innermost loop, then Y, Z, W:
 *  `noiseOut[((w * zCount + z) * yCount + y) * xCount + x]`
 *
 *  @param node         Node handle (must be the root of the node tree).
 *  @param noiseOut     Pre-allocated output array. Must hold at least `xCount * yCount * zCount * wCount` floats.
 *  @param xOffset      Starting X position in world space.
 *  @param yOffset      Starting Y position in world space.
 *  @param zOffset      Starting Z position in world space.
 *  @param wOffset      Starting W position in world space.
 *  @param xCount       Number of samples along the X axis.
 *  @param yCount       Number of samples along the Y axis.
 *  @param zCount       Number of samples along the Z axis.
 *  @param wCount       Number of samples along the W axis.
 *  @param xStepSize    Distance between samples along X.
 *  @param yStepSize    Distance between samples along Y.
 *  @param zStepSize    Distance between samples along Z.
 *  @param wStepSize    Distance between samples along W.
 *  @param seed         Seed value for the noise. Different seeds produce different patterns.
 *  @param outputMinMax Optional float[2] to receive {min, max} of generated values. Pass NULL to skip.
 */
FASTNOISE_API void fnGenUniformGrid4D( const void* node, float* noiseOut,
                                       float xOffset, float yOffset, float zOffset, float wOffset,
                                       int xCount, int yCount, int zCount, int wCount,
                                       float xStepSize, float yStepSize, float zStepSize, float wStepSize,
                                       int seed, float* outputMinMax /*nullptr or float[2]*/ );

/** @brief Generate noise at arbitrary 2D positions.
 *
 *  Evaluates noise at a set of caller-supplied (x, y) positions. Use this when
 *  sampling at non-uniform or scattered locations (e.g. mesh vertices, particle
 *  positions, or any non-grid layout).
 *
 *  This is faster than using fnGenUniformGrid2D since the uniform grid positions
 *  don't need to be generated. So for maximum generation performance across many
 *  generation calls: pre generate your own uniform grid positions once and reuse
 *  them for all generation calls with x/yOffset as needed.
 *
 *  Each position is calculated as `(xPosArray[i] + xOffset, yPosArray[i] + yOffset)`.
 *  The offset parameters allow you to shift all positions without modifying the arrays.
 *
 *  @param node         Node handle (must be the root of the node tree).
 *  @param noiseOut     Pre-allocated output array. Must hold at least @p count floats.
 *  @param count        Number of positions to generate.
 *  @param xPosArray    Array of X coordinates in world space (length >= @p count).
 *  @param yPosArray    Array of Y coordinates in world space (length >= @p count).
 *  @param xOffset      Constant offset added to all X positions.
 *  @param yOffset      Constant offset added to all Y positions.
 *  @param seed         Seed value for the noise. Different seeds produce different patterns.
 *  @param outputMinMax Optional float[2] to receive {min, max} of generated values. Pass NULL to skip.
 */
FASTNOISE_API void fnGenPositionArray2D( const void* node, float* noiseOut, int count,
                                         const float* xPosArray, const float* yPosArray,
                                         float xOffset, float yOffset,
                                         int seed, float* outputMinMax /*nullptr or float[2]*/ );

/** @brief Generate noise at arbitrary 3D positions.
 *
 *  Evaluates noise at a set of caller-supplied (x, y, z) positions. Use this when
 *  sampling at non-uniform or scattered locations (e.g. mesh vertices, particle
 *  positions, or any non-grid layout).
 *
 *  This is faster than using fnGenUniformGrid3D since the uniform grid positions
 *  don't need to be generated. So for maximum generation performance across many
 *  generation calls: pre generate your own uniform grid positions once and reuse
 *  them for all generation calls with x/y/zOffset as needed.
 *
 *  Each position is calculated as `(xPosArray[i] + xOffset, yPosArray[i] + yOffset, zPosArray[i] + zOffset)`.
 *  The offset parameters allow you to shift all positions without modifying the arrays.
 *
 *  @param node         Node handle (must be the root of the node tree).
 *  @param noiseOut     Pre-allocated output array. Must hold at least @p count floats.
 *  @param count        Number of positions to generate.
 *  @param xPosArray    Array of X coordinates in world space (length >= @p count).
 *  @param yPosArray    Array of Y coordinates in world space (length >= @p count).
 *  @param zPosArray    Array of Z coordinates in world space (length >= @p count).
 *  @param xOffset      Constant offset added to all X positions.
 *  @param yOffset      Constant offset added to all Y positions.
 *  @param zOffset      Constant offset added to all Z positions.
 *  @param seed         Seed value for the noise. Different seeds produce different patterns.
 *  @param outputMinMax Optional float[2] to receive {min, max} of generated values. Pass NULL to skip.
 */
FASTNOISE_API void fnGenPositionArray3D( const void* node, float* noiseOut, int count,
                                         const float* xPosArray, const float* yPosArray, const float* zPosArray,
                                         float xOffset, float yOffset, float zOffset,
                                         int seed, float* outputMinMax /*nullptr or float[2]*/ );

/** @brief Generate noise at arbitrary 4D positions.
 *
 *  Evaluates noise at a set of caller-supplied (x, y, z, w) positions. Use this when
 *  sampling at non-uniform or scattered locations (e.g. mesh vertices, particle
 *  positions, or any non-grid layout).
 *
 *  This is faster than using fnGenUniformGrid4D since the uniform grid positions
 *  don't need to be generated. So for maximum generation performance across many
 *  generation calls: pre generate your own uniform grid positions once and reuse
 *  them for all generation calls with x/y/z/wOffset as needed.
 *
 *  Each position is calculated as `(xPosArray[i] + xOffset, yPosArray[i] + yOffset, zPosArray[i] + zOffset, wPosArray[i] + wOffset)`.
 *  The offset parameters allow you to shift all positions without modifying the arrays.
 *
 *  @param node         Node handle (must be the root of the node tree).
 *  @param noiseOut     Pre-allocated output array. Must hold at least @p count floats.
 *  @param count        Number of positions to generate.
 *  @param xPosArray    Array of X coordinates in world space (length >= @p count).
 *  @param yPosArray    Array of Y coordinates in world space (length >= @p count).
 *  @param zPosArray    Array of Z coordinates in world space (length >= @p count).
 *  @param wPosArray    Array of W coordinates in world space (length >= @p count).
 *  @param xOffset      Constant offset added to all X positions.
 *  @param yOffset      Constant offset added to all Y positions.
 *  @param zOffset      Constant offset added to all Z positions.
 *  @param wOffset      Constant offset added to all W positions.
 *  @param seed         Seed value for the noise. Different seeds produce different patterns.
 *  @param outputMinMax Optional float[2] to receive {min, max} of generated values. Pass NULL to skip.
 */
FASTNOISE_API void fnGenPositionArray4D( const void* node, float* noiseOut, int count,
                                         const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray,
                                         float xOffset, float yOffset, float zOffset, float wOffset,
                                         int seed, float* outputMinMax /*nullptr or float[2]*/ );

/** @brief Generate seamlessly tileable 2D noise.
 *
 *  Produces a 2D noise image that tiles perfectly when repeated in both X and Y.
 *  Internally this works by mapping the 2D grid onto a 4D hypertorus, so the
 *  underlying noise types will be using 4D generation.
 *
 *  Output is written in row-major order: `noiseOut[y * xSize + x]`
 *
 *  @param node         Node handle (must be the root of the node tree).
 *  @param noiseOut     Pre-allocated output array. Must hold at least `xSize * ySize` floats.
 *  @param xSize        Number of samples (and tile width) along the X axis.
 *  @param ySize        Number of samples (and tile height) along the Y axis.
 *  @param xStepSize    Distance between samples along X.
 *  @param yStepSize    Distance between samples along Y.
 *  @param seed         Seed value for the noise. Different seeds produce different patterns.
 *  @param outputMinMax Optional float[2] to receive {min, max} of generated values. Pass NULL to skip.
 */
FASTNOISE_API void fnGenTileable2D( const void* node, float* noiseOut,
                                    int xSize, int ySize,
                                    float xStepSize, float yStepSize,
                                    int seed, float* outputMinMax /*nullptr or float[2]*/ );

/** @brief Generate a single 2D noise value at a specific position. VERY SLOW!!!
 *
 *  Avoid using this unless you only need a single sample, this is significantly slower
 *  per-sample than the batch methods (fnGenUniformGrid, fnGenPositionArray) because SIMD
 *  lanes are underutilised. Prefer batch methods for bulk generation.
 *
 *  @param node  Node handle (must be the root of the node tree).
 *  @param x     X position in world space.
 *  @param y     Y position in world space.
 *  @param seed  Seed value for the noise. Different seeds produce different patterns.
 *  @return The noise value at the given position.
 */
FASTNOISE_API float fnGenSingle2D( const void* node, float x, float y, int seed );

/** @brief Generate a single 3D noise value at a specific position. VERY SLOW!!!
 *
 *  Avoid using this unless you only need a single sample, this is significantly slower
 *  per-sample than the batch methods (fnGenUniformGrid, fnGenPositionArray) because SIMD
 *  lanes are underutilised. Prefer batch methods for bulk generation.
 *
 *  @param node  Node handle (must be the root of the node tree).
 *  @param x     X position in world space.
 *  @param y     Y position in world space.
 *  @param z     Z position in world space.
 *  @param seed  Seed value for the noise. Different seeds produce different patterns.
 *  @return The noise value at the given position.
 */
FASTNOISE_API float fnGenSingle3D( const void* node, float x, float y, float z, int seed );

/** @brief Generate a single 4D noise value at a specific position. VERY SLOW!!!
 *
 *  Avoid using this unless you only need a single sample, this is significantly slower
 *  per-sample than the batch methods (fnGenUniformGrid, fnGenPositionArray) because SIMD
 *  lanes are underutilised. Prefer batch methods for bulk generation.
 *
 *  @param node  Node handle (must be the root of the node tree).
 *  @param x     X position in world space.
 *  @param y     Y position in world space.
 *  @param z     Z position in world space.
 *  @param w     W position in world space.
 *  @param seed  Seed value for the noise. Different seeds produce different patterns.
 *  @return The noise value at the given position.
 */
FASTNOISE_API float fnGenSingle4D( const void* node, float x, float y, float z, float w, int seed );

/** @brief Get the total number of registered node types.
 *
 *  Each node type has a unique metadata ID. Use this to determine the valid
 *  range of IDs for functions like fnGetMetadataName() and fnNewFromMetadata().
 *
 *  @return Count of node metadata entries. Valid IDs range from 0 to count-1.
 */
FASTNOISE_API int fnGetMetadataCount();

/** @brief Get the name of a node type by metadata ID.
 *
 *  Returns the class name of the node (e.g. "Simplex", "FractalFBm", "DomainScale").
 *  This is the internal name, not formatted for display. Use
 *  Metadata::FormatMetadataNodeName() in C++ for a display-friendly version.
 *
 *  @param id  Metadata ID (0 to fnGetMetadataCount()-1).
 *  @return Node type name string, or "INVALID NODE ID" if the ID is out of range.
 */
FASTNOISE_API const char* fnGetMetadataName( int id );

/** @brief Create a new node from a metadata ID.
 *
 *  Allows creating nodes dynamically at runtime without compile-time type knowledge.
 *  Configure the returned node by calling fnSetVariableFloat(), fnSetVariableIntEnum(),
 *  fnSetNodeLookup(), fnSetHybridFloat(), and fnSetHybridNodeLookup().
 *
 *  The returned handle must be freed with fnDeleteNodeRef() when no longer needed.
 *
 *  @param id         Metadata ID identifying the node type (0 to fnGetMetadataCount()-1).
 *  @param maxFeatureSet  Maximum auto-detected SIMD feature set. Pass ~0u (uint32_max) for no limit.
 *  @return Opaque node handle, or NULL if the ID is invalid. Must be freed with fnDeleteNodeRef().
 */
FASTNOISE_API void* fnNewFromMetadata( int id, unsigned /*FastSIMD::FeatureSet*/ maxFeatureSet );

/** @brief Get the number of configurable variables on a node type.
 *
 *  Variables are float, int, or enum parameters that control the node's behaviour
 *  (e.g. fractal octaves, lacunarity, noise type). Use this to iterate over all
 *  variables with fnGetMetadataVariableName(), fnGetMetadataVariableType(), etc.
 *
 *  @param id  Metadata ID (0 to fnGetMetadataCount()-1).
 *  @return Number of member variables, or -1 if the ID is invalid.
 */
FASTNOISE_API int fnGetMetadataVariableCount( int id );

/** @brief Get the name of a variable on a node type.
 *
 *  Returns the display name of the variable (e.g. "Octaves", "Lacunarity", "Gain").
 *
 *  @param id             Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param variableIndex  Index into the variables list (0-based, up to fnGetMetadataVariableCount()-1).
 *  @return Variable name string, or an error string if either index is invalid.
 */
FASTNOISE_API const char* fnGetMetadataVariableName( int id, int variableIndex );

/** @brief Get the type of a variable on a node type.
 *
 *  The type determines which setter function to use:
 *  - 0 (EFloat): Use fnSetVariableFloat(). Has min/max bounds from
 *    fnGetMetadataVariableMinFloat() / fnGetMetadataVariableMaxFloat().
 *  - 1 (EInt): Use fnSetVariableIntEnum().
 *  - 2 (EEnum): Use fnSetVariableIntEnum(). Enum option names can be queried
 *    with fnGetMetadataEnumCount() and fnGetMetadataEnumName().
 *
 *  @param id             Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param variableIndex  Index into the variables list (0-based, up to fnGetMetadataVariableCount()-1).
 *  @return Type code (0=float, 1=int, 2=enum), or -1 if either index is invalid.
 */
FASTNOISE_API int fnGetMetadataVariableType( int id, int variableIndex );

/** @brief Get the dimension index for a per-dimension variable.
 *
 *  Some nodes have variables that are registered once per dimension (e.g. X/Y/Z scale).
 *  These share the same base name but differ in their dimension index (0=X, 1=Y, 2=Z, 3=W).
 *  Non-per-dimension variables return -1.
 *
 *  @param id             Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param variableIndex  Index into the variables list (0-based, up to fnGetMetadataVariableCount()-1).
 *  @return Dimension index (0=X, 1=Y, 2=Z, 3=W), or -1 if not a per-dimension variable or if either index is invalid.
 */
FASTNOISE_API int fnGetMetadataVariableDimensionIdx( int id, int variableIndex );

/** @brief Get the number of enum values for an enum variable.
 *
 *  Only meaningful for variables of type EEnum (see fnGetMetadataVariableType()).
 *  Use with fnGetMetadataEnumName() to query option names.
 *
 *  @param id             Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param variableIndex  Index of the enum variable (0-based, up to fnGetMetadataVariableCount()-1).
 *  @return Number of enum options, or -1 if either index is invalid.
 */
FASTNOISE_API int fnGetMetadataEnumCount( int id, int variableIndex );

/** @brief Get the name of an enum option.
 *
 *  Returns the display name for one option of an enum variable. The @p enumIndex
 *  corresponds to the integer value passed to fnSetVariableIntEnum().
 *
 *  @param id             Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param variableIndex  Index of the enum variable (0-based, up to fnGetMetadataVariableCount()-1).
 *  @param enumIndex      Index of the enum option (0-based, up to fnGetMetadataEnumCount()-1).
 *  @return Enum option name string, or an error string if any index is invalid.
 */
FASTNOISE_API const char* fnGetMetadataEnumName( int id, int variableIndex, int enumIndex );

/** @brief Set a float variable on a node instance.
 *
 *  Only valid for variables of type EFloat (see fnGetMetadataVariableType()).
 *
 *  @param node           Node handle created by fnNewFromMetadata() or fnNewFromEncodedNodeTree().
 *  @param variableIndex  Index of the variable to set (0-based, up to fnGetMetadataVariableCount()-1).
 *  @param value          Float value to set.
 *  @return true on success, false if the variable index is out of range or the node type is incorrect.
 */
FASTNOISE_API bool fnSetVariableFloat( void* node, int variableIndex, float value );

/** @brief Set an int or enum variable on a node instance.
 *
 *  Valid for variables of type EInt or EEnum (see fnGetMetadataVariableType()).
 *  For enum variables, the value corresponds to the enum option index
 *  (0 to fnGetMetadataEnumCount()-1).
 *
 *  @param node           Node handle created by fnNewFromMetadata() or fnNewFromEncodedNodeTree().
 *  @param variableIndex  Index of the variable to set (0-based, up to fnGetMetadataVariableCount()-1).
 *  @param value          Integer or enum index value to set.
 *  @return true on success, false if the variable index is out of range or the node type is incorrect.
 */
FASTNOISE_API bool fnSetVariableIntEnum( void* node, int variableIndex, int value );

/** @brief Get the number of required node lookup (source) inputs on a node type.
 *
 *  Node lookups are generator source inputs that MUST be connected to another node
 *  for the owning node to function (e.g. the "Source" input on a Fractal node).
 *  Use with fnGetMetadataNodeLookupName() and fnSetNodeLookup() to query and wire sources.
 *
 *  @param id  Metadata ID (0 to fnGetMetadataCount()-1).
 *  @return Number of node lookup inputs, or -1 if the ID is invalid.
 */
FASTNOISE_API int fnGetMetadataNodeLookupCount( int id );

/** @brief Get the name of a node lookup input on a node type.
 *
 *  Returns the display name of a required generator source input (e.g. "Source", "Warp").
 *
 *  @param id               Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param nodeLookupIndex  Index into the node lookups list (0-based, up to fnGetMetadataNodeLookupCount()-1).
 *  @return Node lookup name string, or an error string if either index is invalid.
 */
FASTNOISE_API const char* fnGetMetadataNodeLookupName( int id, int nodeLookupIndex );

/** @brief Get the dimension index for a per-dimension node lookup.
 *
 *  Some nodes have source inputs registered once per dimension (e.g. per-axis warp sources).
 *  These share the same base name but differ in their dimension index (0=X, 1=Y, 2=Z, 3=W).
 *  Non-per-dimension node lookups return -1.
 *
 *  @param id               Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param nodeLookupIndex  Index into the node lookups list (0-based, up to fnGetMetadataNodeLookupCount()-1).
 *  @return Dimension index (0=X, 1=Y, 2=Z, 3=W), or -1 if not a per-dimension lookup or if either index is invalid.
 */
FASTNOISE_API int fnGetMetadataNodeLookupDimensionIdx( int id, int nodeLookupIndex );

/** @brief Connect a source node to a node lookup input.
 *
 *  Wires a generator node as the source for a required input on the target node.
 *  For example, connecting a Simplex node as the "Source" of a FractalFBm node.
 *
 *  @param node             Target node handle.
 *  @param nodeLookupIndex  Index of the node lookup to set (0-based, up to fnGetMetadataNodeLookupCount()-1).
 *  @param nodeLookup       Source node handle to connect.
 *  @return true on success, false if the index is out of range or the types are incompatible.
 */
FASTNOISE_API bool fnSetNodeLookup( void* node, int nodeLookupIndex, const void* nodeLookup );

/** @brief Get the number of hybrid (constant or node) inputs on a node type.
 *
 *  Hybrid inputs accept either a constant float value or a generator node as their source.
 *  When a generator node is connected it takes priority over the constant value, allowing
 *  parameters like fractal gain or warp amplitude to be driven by another noise source
 *  for spatial variation.
 *
 *  Use with fnGetMetadataHybridName(), fnSetHybridFloat(), and fnSetHybridNodeLookup().
 *
 *  @param id  Metadata ID (0 to fnGetMetadataCount()-1).
 *  @return Number of hybrid inputs, or -1 if the ID is invalid.
 */
FASTNOISE_API int fnGetMetadataHybridCount( int id );

/** @brief Get the name of a hybrid input on a node type.
 *
 *  Returns the display name of a hybrid (constant or node) input (e.g. "Gain", "Amplitude").
 *
 *  @param id          Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param hybridIndex Index into the hybrids list (0-based, up to fnGetMetadataHybridCount()-1).
 *  @return Hybrid input name string, or an error string if either index is invalid.
 */
FASTNOISE_API const char* fnGetMetadataHybridName( int id, int hybridIndex );

/** @brief Get the dimension index for a per-dimension hybrid input.
 *
 *  Some nodes have hybrid inputs registered once per dimension (e.g. per-axis scale).
 *  These share the same base name but differ in their dimension index (0=X, 1=Y, 2=Z, 3=W).
 *  Non-per-dimension hybrids return -1.
 *
 *  @param id          Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param hybridIndex Index into the hybrids list (0-based, up to fnGetMetadataHybridCount()-1).
 *  @return Dimension index (0=X, 1=Y, 2=Z, 3=W), or -1 if not a per-dimension hybrid or if either index is invalid.
 */
FASTNOISE_API int fnGetMetadataHybridDimensionIdx( int id, int hybridIndex );

/** @brief Connect a generator node to a hybrid input (overrides the constant value).
 *
 *  When a generator node is connected, it provides spatially varying values instead
 *  of the constant set by fnSetHybridFloat(). To revert to the constant value
 *  pass NULL as the nodeLookup parameter.
 *
 *  @param node         Target node handle.
 *  @param hybridIndex  Index of the hybrid input (0-based, up to fnGetMetadataHybridCount()-1).
 *  @param nodeLookup   Source node handle to connect.
 *  @return true on success, false if the index is out of range or the types are incompatible.
 */
FASTNOISE_API bool fnSetHybridNodeLookup( void* node, int hybridIndex, const void* nodeLookup );

/** @brief Set the constant float value on a hybrid input.
 *
 *  Sets the constant value used when no generator node is connected to this hybrid input.
 *  If a generator node is connected via fnSetHybridNodeLookup(), it takes priority
 *  over this constant value.
 *
 *  @param node         Target node handle.
 *  @param hybridIndex  Index of the hybrid input (0-based, up to fnGetMetadataHybridCount()-1).
 *  @param value        Constant float value to set.
 *  @return true on success, false if the index is out of range or the node type is incorrect.
 */
FASTNOISE_API bool fnSetHybridFloat( void* node, int hybridIndex, float value );

/** @brief Get the human-readable description of a node type.
 *
 *  Returns the description string from the node's metadata, which explains the
 *  node's behaviour. These descriptions are the same as those shown in the Node
 *  Editor UI and used to populate the wiki Node reference pages.
 *
 *  @param id  Metadata ID (0 to fnGetMetadataCount()-1).
 *  @return Description string, or an error string if the ID is invalid.
 */
FASTNOISE_API const char* fnGetMetadataDescription( int id );

/** @brief Get the number of category groups a node type belongs to.
 *
 *  Nodes are organised into a hierarchy of groups (e.g. "Coherent Noise", "Fractal").
 *  Use this with fnGetMetadataGroupName() to reconstruct the full group path.
 *
 *  @param id  Metadata ID (0 to fnGetMetadataCount()-1).
 *  @return Number of groups, or -1 if the ID is invalid or the node has no groups.
 */
FASTNOISE_API int         fnGetMetadataGroupCount( int id );

/** @brief Get the name of a category group for a node type.
 *
 *  Groups form a hierarchical path. For example, a node with two groups
 *  ["Coherent Noise", "Fractal"] would have groupIndex 0 = "Coherent Noise"
 *  and groupIndex 1 = "Fractal".
 *
 *  @param id          Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param groupIndex  Index of the group (0-based, up to fnGetMetadataGroupCount()-1).
 *  @return Group name string, or an error string if either index is invalid.
 */
FASTNOISE_API const char* fnGetMetadataGroupName( int id, int groupIndex );

/** @brief Get the human-readable description of a variable on a node type.
 *
 *  Returns the description string for a member variable, as shown in the Node
 *  Editor UI tooltips and used to populate the wiki Node reference pages.
 *
 *  @param id             Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param variableIndex  Index into the variables list (0-based).
 *  @return Description string, or an error string if either index is invalid.
 */
FASTNOISE_API const char* fnGetMetadataVariableDescription( int id, int variableIndex );

/** @brief Get the default float value of a variable on a node type.
 *
 *  Only meaningful for variables of type EFloat (see fnGetMetadataVariableType()).
 *  For int/enum variables, use fnGetMetadataVariableDefaultInt() instead.
 *
 *  @param id             Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param variableIndex  Index into the variables list (0-based).
 *  @return Default float value, or 0 if either index is invalid.
 */
FASTNOISE_API float       fnGetMetadataVariableDefaultFloat( int id, int variableIndex );

/** @brief Get the default int/enum value of a variable on a node type.
 *
 *  Only meaningful for variables of type EInt or EEnum (see fnGetMetadataVariableType()).
 *  For float variables, use fnGetMetadataVariableDefaultFloat() instead.
 *
 *  @param id             Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param variableIndex  Index into the variables list (0-based).
 *  @return Default int value, or 0 if either index is invalid.
 */
FASTNOISE_API int         fnGetMetadataVariableDefaultIntEnum( int id, int variableIndex );

/** @brief Get the minimum allowed float value of a variable on a node type.
 *
 *  Returns the lower bound for float variables, used for UI clamping and validation.
 *  Only meaningful for variables of type EFloat (see fnGetMetadataVariableType()).
 *
 *  @param id             Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param variableIndex  Index into the variables list (0-based).
 *  @return Minimum float value, or 0 if either index is invalid.
 */
FASTNOISE_API float       fnGetMetadataVariableMinFloat( int id, int variableIndex );

/** @brief Get the maximum allowed float value of a variable on a node type.
 *
 *  Returns the upper bound for float variables, used for UI clamping and validation.
 *  Only meaningful for variables of type EFloat (see fnGetMetadataVariableType()).
 *
 *  @param id             Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param variableIndex  Index into the variables list (0-based).
 *  @return Maximum float value, or 0 if either index is invalid.
 */
FASTNOISE_API float       fnGetMetadataVariableMaxFloat( int id, int variableIndex );

/** @brief Get the human-readable description of a node lookup input on a node type.
 *
 *  Returns the description string for a required generator source input, as shown
 *  in the Node Editor UI tooltips and used to populate the wiki Node reference pages.
 *
 *  @param id               Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param nodeLookupIndex  Index into the node lookups list (0-based).
 *  @return Description string, or an error string if either index is invalid.
 */
FASTNOISE_API const char* fnGetMetadataNodeLookupDescription( int id, int nodeLookupIndex );

/** @brief Get the human-readable description of a hybrid input on a node type.
 *
 *  Returns the description string for a hybrid (constant or node) input, as shown
 *  in the Node Editor UI tooltips and used to populate the wiki Node reference pages.
 *
 *  @param id          Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param hybridIndex Index into the hybrids list (0-based).
 *  @return Description string, or an error string if either index is invalid.
 */
FASTNOISE_API const char* fnGetMetadataHybridDescription( int id, int hybridIndex );

/** @brief Get the default constant float value of a hybrid input on a node type.
 *  @param id          Metadata ID (0 to fnGetMetadataCount()-1).
 *  @param hybridIndex Index into the hybrids list (0-based).
 *  @return Default float value, or 0 if either index is invalid.
 */
FASTNOISE_API float       fnGetMetadataHybridDefault( int id, int hybridIndex );

#ifdef __cplusplus
}
#endif

#endif
