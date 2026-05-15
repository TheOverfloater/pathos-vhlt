#ifndef VERTEXLIGHT_H__
#define VERTEXLIGHT_H__

#include "vldformat.h"

// Main function to build vertex lighting for all relevant models
void BuildVertexLights();

// Compresses and finalizes the vertex lighting data before writing to BSP
void FinalizeVertexLightBuffers();

// Exports vertex light data to an external .vld file
bool ExportVLDData(vld_datatype_t type);

#endif // VERTEXLIGHT_H__