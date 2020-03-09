//
//  image_tracer.cpp
//  ImageTracer
//
//  Created by Daniel Eke on 09/03/2020.
//  Copyright © 2020 Daniel Eke. All rights reserved.
//

#include "image_tracer.hpp"
#include <map>

namespace IMGTrace
{

ImageTracer::ImageTracer() {}

std::stringstream ImageTracer::processImage(uint8_t* pixels, int width, int height) {
    ImageData data = {
        .width = width,
        .height = height,
        .pixels = pixels
    };
    
    printf("ImageTracer - Color quantization\n");
    IndexedImage ii = colorQuantization(data);
    printf("ImageTracer - Creating layers\n");
    threeDim<int> layers = layering(ii);
    printf("ImageTracer - Scanning paths\n");
    fourDim<int> pathScans = batchPathScan(layers);
    printf("ImageTracer - Interpolating nodes\n");
    fourDim<double> binternodes = batchInternodes(pathScans);
    printf("ImageTracer - Tracing layers\n");
    fourDim<double> tracedLayers = batchTraceLayers(binternodes, 10.0f, 10.0f);
    ii.layers = tracedLayers;
    printf("ImageTracer - Done\n");
    
    return toSvgStringStream(ii);
}

// 1. Color quantization
IndexedImage ImageTracer::colorQuantization(ImageData img) {
    // Only check for black and white colors
    int colorCount = 2;
    std::vector<Color> palette(colorCount);
    palette[0] = {
        .r = 0, .g = 0, .b = 0, .a = 255
    };
    palette[1] = {
        .r = 255, .g = 255, .b = 255, .a = 255
    };

    // Creating indexed color array which has a boundary filled with -1 in every direction
    twoDim<int> array(img.height+2, std::vector<int>(img.width+2,-1));

    for(unsigned int i = 1; i < img.width+1; ++i){
        for(unsigned int j = 1; j < img.height+1; ++j){
            uint8_t* pixel = &img.pixels[((i-1)+((j-1)*img.width))*3];
            uint8_t a = 255 - pixel[0]; //R
            //uint8_t b = 255 - pixel[1]; //G
            //uint8_t c = 255 - pixel[2]; //B
            if (a < 20) {
                array[j][i] = 0;
            } else {
                array[j][i] = 1;
            }
        }
    }
    
    IndexedImage ii = {
        .width = img.width+2,
        .height = img.height+2,
        .array = array,
        .palette = palette,
        .colorCount = colorCount
    };
    
    return ii;
}

// 2. Layer separation and edge detection
// Edge node types ( ▓:light or 1; ░:dark or 0 )
// 12  ░░  ▓░  ░▓  ▓▓  ░░  ▓░  ░▓  ▓▓  ░░  ▓░  ░▓  ▓▓  ░░  ▓░  ░▓  ▓▓
// 48  ░░  ░░  ░░  ░░  ░▓  ░▓  ░▓  ░▓  ▓░  ▓░  ▓░  ▓░  ▓▓  ▓▓  ▓▓  ▓▓
//     0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15
threeDim<int> ImageTracer::layering(IndexedImage ii) {
    // Creating layers for each indexed color in arr
    //int val=0, aw = ii.array[0].length, ah = ii.array.length, n1,n2,n3,n4,n5,n6,n7,n8;
    int val=0, aw = ii.width, ah = ii.height, n1,n2,n3,n4,n5,n6,n7,n8;
    
    threeDim<int> layers(ii.colorCount, twoDim<int>(ah, std::vector<int>(aw)));

    // Looping through all pixels and calculating edge node type
    for(int j=1; j<(ah-1); j++){
        for(int i=1; i<(aw-1); i++){

            // This pixel's indexed color
            val = ii.array[j][i];

            // Are neighbor pixel colors the same?
            n1 = ii.array[j-1][i-1]==val ? 1 : 0;
            n2 = ii.array[j-1][i  ]==val ? 1 : 0;
            n3 = ii.array[j-1][i+1]==val ? 1 : 0;
            n4 = ii.array[j  ][i-1]==val ? 1 : 0;
            n5 = ii.array[j  ][i+1]==val ? 1 : 0;
            n6 = ii.array[j+1][i-1]==val ? 1 : 0;
            n7 = ii.array[j+1][i  ]==val ? 1 : 0;
            n8 = ii.array[j+1][i+1]==val ? 1 : 0;

            // this pixel"s type and looking back on previous pixels
            layers[val][j+1][i+1] = 1 + (n5 * 2) + (n8 * 4) + (n7 * 8) ;
            if(n4==0){ layers[val][j+1][i  ] = 0 + 2 + (n7 * 4) + (n6 * 8) ; }
            if(n2==0){ layers[val][j  ][i+1] = 0 + (n3*2) + (n5 * 4) + 8 ; }
            if(n1==0){ layers[val][j  ][i  ] = 0 + (n2*2) + 4 + (n4 * 8) ; }

        }
    }

    return layers;
}

// Lookup tables for pathscan
int pathscan_dir_lookup[16] = { 0,0,3,0, 1,0,3,0, 0,3,3,1, 0,3,0,0 };
bool pathscan_holepath_lookup[16] = { false,false,false,false, false,false,false,true, false,false,false,true, false,true,true,false };
int pathscan_combined_lookup[16][4][4]  = {
        {{-1,-1,-1,-1}, {-1,-1,-1,-1}, {-1,-1,-1,-1}, {-1,-1,-1,-1}},// arr[py][px]==0 is invalid
        {{ 0, 1, 0,-1}, {-1,-1,-1,-1}, {-1,-1,-1,-1}, { 0, 2,-1, 0}},
        {{-1,-1,-1,-1}, {-1,-1,-1,-1}, { 0, 1, 0,-1}, { 0, 0, 1, 0}},
        {{ 0, 0, 1, 0}, {-1,-1,-1,-1}, { 0, 2,-1, 0}, {-1,-1,-1,-1}},

        {{-1,-1,-1,-1}, { 0, 0, 1, 0}, { 0, 3, 0, 1}, {-1,-1,-1,-1}},
        {{13, 3, 0, 1}, {13, 2,-1, 0}, { 7, 1, 0,-1}, { 7, 0, 1, 0}},
        {{-1,-1,-1,-1}, { 0, 1, 0,-1}, {-1,-1,-1,-1}, { 0, 3, 0, 1}},
        {{ 0, 3, 0, 1}, { 0, 2,-1, 0}, {-1,-1,-1,-1}, {-1,-1,-1,-1}},

        {{ 0, 3, 0, 1}, { 0, 2,-1, 0}, {-1,-1,-1,-1}, {-1,-1,-1,-1}},
        {{-1,-1,-1,-1}, { 0, 1, 0,-1}, {-1,-1,-1,-1}, { 0, 3, 0, 1}},
        {{11, 1, 0,-1}, {14, 0, 1, 0}, {14, 3, 0, 1}, {11, 2,-1, 0}},
        {{-1,-1,-1,-1}, { 0, 0, 1, 0}, { 0, 3, 0, 1}, {-1,-1,-1,-1}},

        {{ 0, 0, 1, 0}, {-1,-1,-1,-1}, { 0, 2,-1, 0}, {-1,-1,-1,-1}},
        {{-1,-1,-1,-1}, {-1,-1,-1,-1}, { 0, 1, 0,-1}, { 0, 0, 1, 0}},
        {{ 0, 1, 0,-1}, {-1,-1,-1,-1}, {-1,-1,-1,-1}, { 0, 2,-1, 0}},
        {{-1,-1,-1,-1}, {-1,-1,-1,-1}, {-1,-1,-1,-1}, {-1,-1,-1,-1}}// arr[py][px]==15 is invalid
};

// 3. Walking through an edge node array, discarding edge node types 0 and 15 and creating paths
// from the rest.
// Walk directions (dir): 0 > ; 1 ^ ; 2 < ; 3 v
// Edge node types ( ▓:light or 1; ░:dark or 0 )
// ░░  ▓░  ░▓  ▓▓  ░░  ▓░  ░▓  ▓▓  ░░  ▓░  ░▓  ▓▓  ░░  ▓░  ░▓  ▓▓
// ░░  ░░  ░░  ░░  ░▓  ░▓  ░▓  ░▓  ▓░  ▓░  ▓░  ▓░  ▓▓  ▓▓  ▓▓  ▓▓
// 0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15
//
fourDim<int> ImageTracer::batchPathScan(threeDim<int> layers) {
    
    fourDim<int> pathscans;

    for (auto& arr : layers) {
        threeDim<int> paths;
        twoDim<int> thisPath;
        int px = 0, py = 0, w = arr[0].size(), h = arr.size(), dir = 0;
        bool pathfinished = true, holepath = false;
        int* lookuprow = pathscan_combined_lookup[0][0];
        float pathomit = 1.0f;
        
        for(int j=0;j<h;j++){
            for(int i=0;i<w;i++){
                if((arr[j][i]!=0)&&(arr[j][i]!=15)){

                    // Init
                    px = i; py = j;
                    //printf("py: %d ", py);
                    //printf("j: %d \n", j);
                    thisPath = twoDim<int>();
                    pathfinished = false;

                    // fill paths will be drawn, but hole paths are also required to remove unnecessary edge nodes
                    dir = pathscan_dir_lookup[ arr[py][px] ]; holepath = pathscan_holepath_lookup[ arr[py][px] ];

                    // Path points loop
                    while(!pathfinished) { // && px >= 0 && py >= 0 && px < w && py < h && arr[py][px] != 0 && arr[py][px] != 15){
                
                        int type = arr[py][px];
                        // New path point
                        auto point = std::vector<int>(3);
                        point[0] = px-1;
                        point[1] = py-1;
                        point[2] = type;
                        thisPath.push_back(point);

                        // Next: look up the replacement, direction and coordinate changes = clear this cell, turn if required, walk forward
                        lookuprow = pathscan_combined_lookup[ type ][ dir ];
                        arr[py][px] = lookuprow[0]; dir = lookuprow[1]; px += lookuprow[2]; py += lookuprow[3];

                        // Close path
                        if(((px-1)==thisPath[0][0])&&((py-1)==thisPath[0][1])){
                            pathfinished = true;
                            // Discarding 'hole' type paths and paths shorter than pathomit
                            if( (holepath) || (thisPath.size() < pathomit) ){
                                // Do nothing
                                //paths.erase(thispath);
                            } else {
                                paths.push_back(thisPath);
                            }
                        }

                    }
                }
            }
        }
        pathscans.push_back(paths);
    }
    
    return pathscans;
}

// 4. interpolating between path points for nodes with 8 directions ( East, SouthEast, S, SW, W, NW, N, NE )
fourDim<double> ImageTracer::batchInternodes(fourDim<int> bPaths) {
    fourDim<double> binternodes;

    for (auto& paths : bPaths) {
        threeDim<double> ins;
        twoDim<double> thisinp;
        std::vector<double> thisPoint = std::vector<double>(2);
        std::vector<double> nextPoint = std::vector<double>(2);
        std::vector<int> pp1, pp2, pp3;
        int palen = 0, nextidx = 0, nextidx2 = 0;
        
        for (int pacnt = 0; pacnt < paths.size(); pacnt++) {
            thisinp = twoDim<double>();
            palen = paths[pacnt].size();
            
            // pathpoints loop
            for (int pcnt = 0; pcnt < palen; pcnt++) {

                // interpolate between two path points
                nextidx = (pcnt + 1) % palen;
                nextidx2 = (pcnt + 2) % palen;
                thisPoint = std::vector<double>(3);
                pp1 = paths[pacnt][pcnt];
                pp2 = paths[pacnt][nextidx];
                pp3 = paths[pacnt][nextidx2];
                thisPoint[0] = (pp1[0] + pp2[0]) / 2.0;
                thisPoint[1] = (pp1[1] + pp2[1]) / 2.0;
                nextPoint[0] = (pp2[0] + pp3[0]) / 2.0;
                nextPoint[1] = (pp2[1] + pp3[1]) / 2.0;
                // line segment direction to the next point
                if (thisPoint[0] < nextPoint[0]) {
                    if (thisPoint[1] < nextPoint[1]) {
                        thisPoint[2] = 1.0;
                    } // SouthEast
                    else if (thisPoint[1] > nextPoint[1]) {
                        thisPoint[2] = 7.0;
                    } // NE
                    else {
                        thisPoint[2] = 0.0;
                    } // E
                } else if (thisPoint[0] > nextPoint[0]) {
                    if (thisPoint[1] < nextPoint[1]) {
                        thisPoint[2] = 3.0;
                    } // SW
                    else if (thisPoint[1] > nextPoint[1]) {
                        thisPoint[2] = 5.0;
                    } // NW
                    else {
                        thisPoint[2] = 4.0;
                    } // W
                } else {
                    if (thisPoint[1] < nextPoint[1]) {
                        thisPoint[2] = 2.0;
                    } // S
                    else if (thisPoint[1] > nextPoint[1]) {
                        thisPoint[2] = 6.0;
                    } // N
                    else {
                        thisPoint[2] = 8.0;
                    } // center, this should not happen
                }
                thisinp.push_back(thisPoint);
            }
            ins.push_back(thisinp);
        }
        
        binternodes.push_back(ins);
    }
    return binternodes;
}

// 5. tracepath() : recursively trying to fit straight and quadratic spline segments on the 8
// direction internode path

// 5.1. Find sequences of points with only 2 segment types
// 5.2. Fit a straight line on the sequence
// 5.3. If the straight line fails (an error>ltreshold), find the point with the biggest error
// 5.4. Fit a quadratic spline through errorpoint (project this to get controlpoint), then measure
// errors on every point in the sequence
// 5.5. If the spline fails (an error>qtreshold), find the point with the biggest error, set
// splitpoint = (fitting point + errorpoint)/2
// 5.6. Split sequence and recursively apply 5.2. - 5.7. to startpoint-splitpoint and
// splitpoint-endpoint sequences
// 5.7. TODO? If splitpoint-endpoint is a spline, try to add new points from the next sequence

// This returns an SVG Path segment as a double[7] where
// segment[0] ==1.0 linear  ==2.0 quadratic interpolation
// segment[1] , segment[2] : x1 , y1
// segment[3] , segment[4] : x2 , y2 ; middle point of Q curve, endpoint of L line
// segment[5] , segment[6] : x3 , y3 for Q curve, should be 0.0 , 0.0 for L line
//
// path type is discarded, no check for path.size < 3 , which should not happen
fourDim<double> ImageTracer::batchTraceLayers(fourDim<double> binternodes, float ltreshold, float qtreshold) {
    fourDim<double> btbis;
    
    for (auto& internodepaths : binternodes) {
        threeDim<double> btracedpaths;

        for (auto& path : internodepaths) {
            int pcnt = 0, seqend = 0;
            double segtype1, segtype2;
            twoDim<double> smp;
            
            // Double [] thissegment;
            int pathlength = path.size();

            while (pcnt < pathlength) {
              // 5.1. Find sequences of points with only 2 segment types
              segtype1 = path[pcnt][2];
              segtype2 = -1;
              seqend = pcnt + 1;
              while (((path[seqend][2] == segtype1)
                      || (path[seqend][2] == segtype2)
                      || (segtype2 == -1))
                  && (seqend < (pathlength - 1))) {
                if ((path[seqend][2] != segtype1) && (segtype2 == -1)) {
                  segtype2 = path[seqend][2];
                }
                seqend++;
              }
              if (seqend == (pathlength - 1)) {
                seqend = 0;
              }

              // 5.2. - 5.6. Split sequence and recursively apply 5.2. - 5.6. to startpoint-splitpoint and
              // splitpoint-endpoint sequences
              auto segment = fitseq(path, ltreshold, qtreshold, pcnt, seqend);
              for (auto& thissegment : segment) {
                  smp.push_back(thissegment);
              }
                
              // 5.7. TODO? If splitpoint-endpoint is a spline, try to add new points from the next sequence

              // forward pcnt;
              if (seqend > 0) {
                pcnt = seqend;
              } else {
                pcnt = pathlength;
              }
            } // End of pcnt loop

            btracedpaths.push_back(smp);
        }
        
        btbis.push_back(btracedpaths);
    }
    
    return btbis;
}

twoDim<double> ImageTracer::fitseq(twoDim<double> path, float ltreshold, float qtreshold, int seqstart, int seqend) {
    twoDim<double> segment;
    std::vector<double> thisSegment;
    int pathlength = path.size();

    
    // return if invalid seqend
    if ((seqend > pathlength) || (seqend < 0)) {
      return segment;
    }

    int errorpoint = seqstart;
    bool curvepass = true;
    double px, py, dist2, errorval = 0;
    double tl = (seqend - seqstart);
    if (tl < 0) {
      tl += pathlength;
    }
    double vx = (path[seqend][0] - path[seqstart][0]) / tl,
        vy = (path[seqend][1] - path[seqstart][1]) / tl;

    // 5.2. Fit a straight line on the sequence
    int pcnt = (seqstart + 1) % pathlength;
    double pl;
    while (pcnt != seqend) {
      pl = pcnt - seqstart;
      if (pl < 0) {
        pl += pathlength;
      }
      px = path[seqstart][0] + (vx * pl);
      py = path[seqstart][1] + (vy * pl);
      dist2 =
          ((path[pcnt][0] - px) * (path[pcnt][0] - px))
              + ((path[pcnt][1] - py) * (path[pcnt][1] - py));
      if (dist2 > ltreshold) {
        curvepass = false;
      }
      if (dist2 > errorval) {
        errorpoint = pcnt;
        errorval = dist2;
      }
      pcnt = (pcnt + 1) % pathlength;
    }

    // return straight line if fits
    if (curvepass) {
        thisSegment = std::vector<double>(7);
        thisSegment[0] = 1.0;
        thisSegment[1] = path[seqstart][0];
        thisSegment[2] = path[seqstart][1];
        thisSegment[3] = path[seqend][0];
        thisSegment[4] = path[seqend][1];
        thisSegment[5] = 0.0;
        thisSegment[6] = 0.0;
        segment.push_back(thisSegment);
      return segment;
    }

    // 5.3. If the straight line fails (an error>ltreshold), find the point with the biggest error
    int fitpoint = errorpoint;
    curvepass = true;
    errorval = 0;

    // 5.4. Fit a quadratic spline through this point, measure errors on every point in the sequence
    // helpers and projecting to get control point
    double t = (fitpoint - seqstart) / tl,
        t1 = (1.0 - t) * (1.0 - t),
        t2 = 2.0 * (1.0 - t) * t,
        t3 = t * t;
    double
        cpx =
            (((t1 * path[seqstart][0]) + (t3 * path[seqend][0])) - path[fitpoint][0])
                / -t2,
        cpy =
            (((t1 * path[seqstart][1]) + (t3 * path[seqend][1])) - path[fitpoint][1])
                / -t2;

    // Check every point
    pcnt = seqstart + 1;
    while (pcnt != seqend) {

      t = (pcnt - seqstart) / tl;
      t1 = (1.0 - t) * (1.0 - t);
      t2 = 2.0 * (1.0 - t) * t;
      t3 = t * t;
      px = (t1 * path[seqstart][0]) + (t2 * cpx) + (t3 * path[seqend][0]);
      py = (t1 * path[seqstart][1]) + (t2 * cpy) + (t3 * path[seqend][1]);

      dist2 =
          ((path[pcnt][0] - px) * (path[pcnt][0] - px))
              + ((path[pcnt][1] - py) * (path[pcnt][1] - py));

      if (dist2 > qtreshold) {
        curvepass = false;
      }
      if (dist2 > errorval) {
        errorpoint = pcnt;
        errorval = dist2;
      }
      pcnt = (pcnt + 1) % pathlength;
    }

    // return spline if fits
    if (curvepass) {
        thisSegment = std::vector<double>(7);
        thisSegment[0] = 2.0;
        thisSegment[1] = path[seqstart][0];
        thisSegment[2] = path[seqstart][1];
        thisSegment[3] = cpx;
        thisSegment[4] = cpy;
        thisSegment[5] = path[seqend][0];
        thisSegment[6] = path[seqend][1];
        segment.push_back(thisSegment);
      return segment;
    }

    // 5.5. If the spline fails (an error>qtreshold), find the point with the biggest error,
    // set splitpoint = (fitting point + errorpoint)/2
    int splitpoint = (fitpoint + errorpoint) / 2;

    // 5.6. Split sequence and recursively apply 5.2. - 5.6. to startpoint-splitpoint and
    // splitpoint-endpoint sequences
    segment = fitseq(path, ltreshold, qtreshold, seqstart, splitpoint);
    auto splipointSegment = fitseq(path, ltreshold, qtreshold, splitpoint, seqend);
    for (auto& thissegment : segment) {
        segment.push_back(thissegment);
    }

    return segment;
}

bool mapContainsKey(std::map<double, std::vector<int>>& map, double key)
{
  if (map.find(key) == map.end()) return false;
  return true;
}

std::stringstream ImageTracer::toSvgStringStream(IndexedImage ii) {
    float scale = 1.0;
    // SVG start
    int w = (int) (ii.width * scale), h = (int) (ii.height * scale);
    std::stringstream ss;
    ss << "<svg " << "width=\"" << w << "\" height=\"" << h << "\" ";
    ss << "version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">";

    // creating Z-index
    std::map<double, std::vector<int>> zindex;
    double label;
    // Layer loop
    for (int k = 0; k < ii.layers.size(); k++) {
      // Path loop
      for (int pcnt = 0; pcnt < ii.layers[k].size(); pcnt++) {
        // Label (Z-index key) is the startpoint of the path, linearized
        label = (ii.layers[k][pcnt][0][2] * w) + ii.layers[k][pcnt][0][1];
        // Creating new list if required
          if(!mapContainsKey(zindex, label)) {
              zindex[label] = std::vector<int>(2);
          }
          // Adding layer and path number to list
          zindex[label][0] = k;
          zindex[label][1] = pcnt;
      }
    }

    // Sorting Z-index is not required, TreeMap is sorted automatically

    // Drawing
    // Z-index loop
    for (auto const& x : zindex) {
        auto value = x.second;
        twoDim<double> segments = ii.layers[value[0]][value[1]];
        std::string colorstr = value[0] == 0 ?
        "fill=\"rgb(255,255,255)\" stroke=\"rgb(0,0,0)\" opacity=\"1\" " :
        "fill=\"rgb(0,0,0)\" stroke=\"rgb(255,255,255)\" opacity=\"1\" ";
        // Path
        ss << "<path " << colorstr << "d=\"" << "M " << (segments[0][1] * scale) << " " << segments[0][2] * scale << " ";

          for (int pcnt = 0; pcnt < segments.size(); pcnt++) {
            if (segments[pcnt][0] == 1.0) {
                ss << "L ";
                ss << (segments[pcnt][3] * scale);
                ss << " ";
                ss << (segments[pcnt][4] * scale);
                ss << " ";
            } else {
                ss << "Q ";
                ss << (segments[pcnt][3] * scale);
                ss << " ";
                ss << (segments[pcnt][4] * scale);
                ss << " ";
                ss << (segments[pcnt][5] * scale);
                ss << " ";
                ss << (segments[pcnt][6] * scale);
                ss << " ";
            }
          }
        

        ss << "Z\" />";
    }

    // SVG End
    ss << "</svg>";
    
    return ss;
}

}
