//
//  main.cpp
//  ImageTracer
//
//  Created by Daniel Eke on 09/03/2020.
//  Copyright © 2020 Daniel Eke. All rights reserved.
//

#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "image_tracer.hpp"
#include <unistd.h>
#include <string>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>

int main(int argc, const char * argv[]) {
    ImageTracer tracer = ImageTracer();
    
    int width, height, bpp;
    unsigned char* rgb = stbi_load( "./testimages/11.png", &width, &height, &bpp, 3 );
    std::stringstream result = tracer.processImage(rgb, width, height);
    stbi_image_free( rgb );
    
    std::ofstream outFile("./out/test.svg");
    outFile << result.rdbuf();
    outFile.close();

    return 0;
}


