#include "ModelLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

// Defining here as can only be defined once, we use tinygltf in Model.h
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h" // Includes stb_image and stb_image_write