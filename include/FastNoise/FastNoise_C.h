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
 *  @param simdLevel      Maximum SIMD feature set. Pass ~0u (uint32_max) for auto-detection.
 *  @return Opaque node handle, or NULL on failure.
 */
FASTNOISE_API void* fnNewFromEncodedNodeTree( const char* encodedString, unsigned /*FastSIMD::FeatureSet*/ simdLevel /*~0u = Auto*/ );

/** @brief Release a node handle previously obtained from fnNewFromEncodedNodeTree() or fnNewFromMetadata().
 *  @param node  Node handle to release. May be NULL (no-op).
 */
FASTNOISE_API void fnDeleteNodeRef( void* node );

/** @brief Get the SIMD feature set level active for this node.
 *  @param node  Node handle.
 *  @return SIMD level as a FastSIMD::FeatureSet value.
 */
FASTNOISE_API unsigned fnGetSIMDLevel( const void* node );

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
 *  @return Count of node metadata entries. Valid IDs range from 0 to count-1.
 */
FASTNOISE_API int fnGetMetadataCount();

/** @brief Get the name of a node type by metadata ID.
 *  @param id  Metadata ID (0 to fnGetMetadataCount()-1).
 *  @return Node type name string (e.g. "Simplex", "FractalFBm").
 */
FASTNOISE_API const char* fnGetMetadataName( int id );

/** @brief Create a new node from a metadata ID.
 *  @param id         Metadata ID identifying the node type.
 *  @param simdLevel  Maximum SIMD feature set. Pass ~0u for auto-detection.
 *  @return Opaque node handle. Must be freed with fnDeleteNodeRef().
 */
FASTNOISE_API void* fnNewFromMetadata( int id, unsigned /*FastSIMD::FeatureSet*/ simdLevel /*~0u = Auto*/ );

/** @brief Get the number of configurable variables on a node type.
 *  @param id  Metadata ID.
 *  @return Number of member variables.
 */
FASTNOISE_API int fnGetMetadataVariableCount( int id );

/** @brief Get the name of a variable on a node type.
 *  @param id             Metadata ID.
 *  @param variableIndex  Index into the variables list (0-based).
 *  @return Variable name string.
 */
FASTNOISE_API const char* fnGetMetadataVariableName( int id, int variableIndex );

/** @brief Get the type of a variable (0=float, 1=int, 2=enum).
 *  @param id             Metadata ID.
 *  @param variableIndex  Index into the variables list.
 *  @return Type code corresponding to Metadata::MemberVariable::eType.
 */
FASTNOISE_API int fnGetMetadataVariableType( int id, int variableIndex );

/** @brief Get the dimension index for a per-dimension variable, or -1 if not per-dimension. */
FASTNOISE_API int fnGetMetadataVariableDimensionIdx( int id, int variableIndex );

/** @brief Get the number of enum values for an enum variable.
 *  @param id             Metadata ID.
 *  @param variableIndex  Index of the enum variable.
 *  @return Number of enum options.
 */
FASTNOISE_API int fnGetMetadataEnumCount( int id, int variableIndex );

/** @brief Get the name of an enum option.
 *  @param id             Metadata ID.
 *  @param variableIndex  Index of the enum variable.
 *  @param enumIndex      Index of the enum option (0-based).
 *  @return Enum option name string.
 */
FASTNOISE_API const char* fnGetMetadataEnumName( int id, int variableIndex, int enumIndex );

/** @brief Set a float variable on a node instance.
 *  @param node           Node handle.
 *  @param variableIndex  Index of the variable to set.
 *  @param value          Float value.
 *  @return true on success, false if the variable index or type is invalid.
 */
FASTNOISE_API bool fnSetVariableFloat( void* node, int variableIndex, float value );

/** @brief Set an int or enum variable on a node instance.
 *  @param node           Node handle.
 *  @param variableIndex  Index of the variable to set.
 *  @param value          Integer/enum value.
 *  @return true on success, false if the variable index or type is invalid.
 */
FASTNOISE_API bool fnSetVariableIntEnum( void* node, int variableIndex, int value );

/** @brief Get the number of required node lookup (source) inputs on a node type. */
FASTNOISE_API int fnGetMetadataNodeLookupCount( int id );

/** @brief Get the name of a node lookup input. */
FASTNOISE_API const char* fnGetMetadataNodeLookupName( int id, int nodeLookupIndex );

/** @brief Get the dimension index for a per-dimension node lookup, or -1 if not per-dimension. */
FASTNOISE_API int fnGetMetadataNodeLookupDimensionIdx( int id, int nodeLookupIndex );

/** @brief Connect a source node to a node lookup input.
 *  @param node             Target node handle.
 *  @param nodeLookupIndex  Index of the node lookup to set.
 *  @param nodeLookup       Source node handle to connect.
 *  @return true on success, false if the types are incompatible.
 */
FASTNOISE_API bool fnSetNodeLookup( void* node, int nodeLookupIndex, const void* nodeLookup );

/** @brief Get the number of hybrid (constant or node) inputs on a node type. */
FASTNOISE_API int fnGetMetadataHybridCount( int id );

/** @brief Get the name of a hybrid input. */
FASTNOISE_API const char* fnGetMetadataHybridName( int id, int hybridIndex );

/** @brief Get the dimension index for a per-dimension hybrid input, or -1 if not per-dimension. */
FASTNOISE_API int fnGetMetadataHybridDimensionIdx( int id, int hybridIndex );

/** @brief Connect a generator node to a hybrid input (overrides the constant value).
 *  @param node         Target node handle.
 *  @param hybridIndex  Index of the hybrid input.
 *  @param nodeLookup   Source node handle to connect.
 *  @return true on success, false if the types are incompatible.
 */
FASTNOISE_API bool fnSetHybridNodeLookup( void* node, int hybridIndex, const void* nodeLookup );

/** @brief Set the constant float value on a hybrid input.
 *  @param node         Target node handle.
 *  @param hybridIndex  Index of the hybrid input.
 *  @param value        Constant float value.
 *  @return true on success.
 */
FASTNOISE_API bool fnSetHybridFloat( void* node, int hybridIndex, float value );

// Rich metadata queries
FASTNOISE_API const char* fnGetMetadataDescription( int id );
FASTNOISE_API int         fnGetMetadataGroupCount( int id );
FASTNOISE_API const char* fnGetMetadataGroupName( int id, int groupIndex );

FASTNOISE_API const char* fnGetMetadataVariableDescription( int id, int variableIndex );
FASTNOISE_API float       fnGetMetadataVariableDefaultFloat( int id, int variableIndex );
FASTNOISE_API int         fnGetMetadataVariableDefaultInt( int id, int variableIndex );
FASTNOISE_API float       fnGetMetadataVariableMinFloat( int id, int variableIndex );
FASTNOISE_API float       fnGetMetadataVariableMaxFloat( int id, int variableIndex );

FASTNOISE_API const char* fnGetMetadataNodeLookupDescription( int id, int nodeLookupIndex );

FASTNOISE_API const char* fnGetMetadataHybridDescription( int id, int hybridIndex );
FASTNOISE_API float       fnGetMetadataHybridDefault( int id, int hybridIndex );

#ifdef __cplusplus
}
#endif

#endif
