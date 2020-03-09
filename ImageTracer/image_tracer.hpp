//
//  image_tracer.hpp
//  ImageTracer
//
//  Created by Daniel Eke on 09/03/2020.
//  Copyright © 2020 Daniel Eke. All rights reserved.
//

#ifndef image_tracer_hpp
#define image_tracer_hpp

#include <stdio.h>
#include <vector>
#include <iostream>
#include <sstream>

struct ImageData {
    int width, height;
    uint8_t* pixels;
};

struct IndexedImage{
    int width, height, colorCount;
    std::vector<std::vector<int>> array; // array[x][y] of palette colors
    std::vector<std::vector<int>> palette;// array[palettelength][4] RGBA color palette
    std::vector<std::vector<std::vector<std::vector<double>>>> layers;// tracedata
};

class ImageTracer{

    public:
    
    ImageTracer();
    
    std::stringstream processImage(uint8_t* pixels, int width, int height);
    
private:
    
    IndexedImage colorQuantization(ImageData img);
    std::vector<std::vector<std::vector<int>>> layering(IndexedImage ii);
    std::vector<std::vector<std::vector<std::vector<int>>>> batchPathScan(std::vector<std::vector<std::vector<int>>> layers);
    std::vector<std::vector<std::vector<std::vector<double>>>> batchInternodes(std::vector<std::vector<std::vector<std::vector<int>>>> bPaths);
    std::vector<std::vector<std::vector<std::vector<double>>>> batchTraceLayers(std::vector<std::vector<std::vector<std::vector<double>>>> binternodes, float ltreshold, float qtreshold);
    std::vector<std::vector<double>> fitseq(std::vector<std::vector<double>> path, float ltreshold, float qtreshold, int seqstart, int seqend);
    std::stringstream toSvgStringStream(IndexedImage ii);

};


#endif /* image_tracer_hpp */
