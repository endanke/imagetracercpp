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

namespace IMGTrace
{

struct ImageData {
    int width, height;
    uint8_t* pixels;
};

struct Color {
    int r, g, b, a;
};

struct Pixel {
    int c = -1; // Index of color in palette
    unsigned int x, y;
};

template <typename T>
using twoDim = std::vector<std::vector<T>>;
template <typename T>
using threeDim = std::vector<std::vector<std::vector<T>>>;
template <typename T>
using fourDim = std::vector<std::vector<std::vector<std::vector<T>>>>;

struct IndexedImage {
    int width, height, colorCount;
    std::vector<Color> palette;// array[palettelength][4] RGBA color palette
    twoDim<int> array; // array[x][y] of palette colors
    fourDim<double> layers;// tracedata
};

class ImageTracer{

    public:
    
    ImageTracer();
    
    std::stringstream processImage(uint8_t* pixels, int width, int height);
    
private:
    
    IndexedImage colorQuantization(ImageData img);
    threeDim<int> layering(IndexedImage ii);
    fourDim<int> batchPathScan(threeDim<int> layers);
    fourDim<double> batchInternodes(fourDim<int> bPaths);
    fourDim<double> batchTraceLayers(fourDim<double> binternodes, float ltreshold, float qtreshold);
    twoDim<double> fitseq(twoDim<double> path, float ltreshold, float qtreshold, int seqstart, int seqend);
    std::stringstream toSvgStringStream(IndexedImage ii);

};

}

#endif /* image_tracer_hpp */
