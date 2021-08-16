#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
//#include "stb_image_resize.h"

#include <stdlib.h>

#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

bool build_cmap( char * oclassf, char * nclassf, std::map<int,int>& cmap ){
    int ccount = 0;
    std::map<std::string,int> nmap; // map name to old value 
    std::ifstream ocf, ncf;
    std::string line;          // buffer 

    ocf.open(oclassf);
    while( std::getline( ocf, line ) ){
        if( line.empty() ) continue;
        std::istringstream lstr(line);
        std::string cname;
        int r,g,b;
        if( !(lstr >> r) || r < 0 || r > 256 ){ printf("missing or invalid R-value\n"); goto fail; }
        if( !(lstr >> g) || g < 0 || g > 256 ){ printf("missing or invalid G-value\n"); goto fail; }
        if( !(lstr >> b) || b < 0 || b > 256 ){ printf("missing or invalid B-value\n"); goto fail; }
        if( !(lstr >> cname) ){ printf("missing class name\n"); goto fail; }

//        printf("%4d %4d %4d %s\n", r, g, b, cname.c_str());

        if( nmap.end() != nmap.find(cname) ){ printf("redefine value of class \'%s\' in \'%s\'\n", cname.c_str(), oclassf ); goto fail; }
        nmap[cname] = (r<<16) | (g<<8) | b;

//        printf("%4d %4d %4d %s => %8d\n", r, g, b, cname.c_str(), nmap[cname]);
    }
    ocf.close();

    ncf.open(nclassf);
    ccount = nmap.size();
    while( std::getline( ncf, line ) ){
        if( line.empty() ) continue;
        std::istringstream lstr(line);
        std::string cname;
        if( !(lstr >> cname) ){ printf("missing class name\n"); goto fail; }

        int old_value = nmap[cname];
        int r,g,b;
        r = (old_value & (255<<16))>>16;
        g = (old_value & (255<< 8))>> 8;
        b = (old_value & (255<< 0))>> 0;

//        printf("%8d = %4d %4d %4d %s\n", old_value, r, g, b, cname.c_str());
        if( nmap.end() == nmap.find(cname) ){ printf("cannot find class \'%s\' in old class definition\n", cname.c_str() ); goto fail; }
        if( cmap.end() != cmap.find(old_value) ){ printf("remap class \'(%3d,%3d,%3d)\' in new class definition\n", r,g,b ); goto fail; }
//        printf("remap %d %d %d to %d\n", r,g,b, ccount );
        cmap[ old_value ] = ccount -- ;
    }
    ncf.close();

    return 1;
fail:
    return -1;
}

void nlsb0(int nzero, char * filename, char * oimagef){
    int channels = 0;
    int w, h, c;
    unsigned char *data = stbi_load(filename, &w, &h, &c, channels);
    if (!data) {
        fprintf(stderr, "Cannot load image \"%s\"\nSTB Reason: %s\n", filename, stbi_failure_reason());
        exit(0);
    }
    unsigned int mask = ~0 << nzero;
    for( int i = 0; i < w*h*c; i ++ )
        data[i] &= mask;

    char * pos = strrchr(oimagef,'.');
    if( pos ){
        int res = 0;
        pos ++; // point to char after .
        for(char * c = pos; '\0'!=*c; c ++ ) *c = tolower(*c);
        if( !strcmp(pos,"jpg") )
            res = stbi_write_jpg(oimagef, w, h, c, data, 100);
        else
        if( !strcmp(pos,"png") )
            res = stbi_write_png(oimagef, w, h, c, data, w*c);
        else
        if( !strcmp(pos,"bmp") )
            res = stbi_write_bmp(oimagef, w, h, c, data);
        else
        if( !strcmp(pos,"tga") )
            res = stbi_write_tga(oimagef, w, h, c, data);
        else
            printf("unsupported file extension \'.%s\'\n", pos);

        if(res){
            printf("From \'%s\' (%d,%d,%d) to \'%s\'\n", filename, w,h,c, oimagef );
        } else
            printf("writting fail: \'%s\'\n", oimagef);

    } else {
        printf("require extension specified in the given output filename '%s' \n", oimagef);
    }

    free(data);
}

int latrans(char * filename, char * oclassf, char * nclassf, char * oimagef ){
    int w, h, c;
    int nError = 0;
    printf("name: img= %s wclass= %s %s \n", filename, oclassf, nclassf);

    std::map<int,int> cmap;
    if( -1 == build_cmap( oclassf, nclassf, cmap ) )
        return 0;



    int channels = 0;
    unsigned char *data = stbi_load(filename, &w, &h, &c, channels);
    if (!data) {
        fprintf(stderr, "Cannot load image \"%s\"\nSTB Reason: %s\n", filename, stbi_failure_reason());
        exit(0);
    }

    std::vector<unsigned char> oimage( w * h, 0 );
    for( int i = 0; i < w*h; i ++ ){
        int r,g,b;
        int idx = i*3;
        r = data[idx+0];
        g = data[idx+1];
        b = data[idx+2];
        int new_value;
        int old_value = (r<<16) | (g<<8) | b;
        if( cmap.end() == cmap.find(old_value) ){
            printf("cannot find class \'(%3d,%3d,%3d)\' in class map\n", r,g,b );
            //goto fail;
            nError ++ ;
            new_value = 0;
            //continue;
        } else
            new_value = cmap[old_value];
        oimage[i] = new_value;
    }
    {
        int stride_in_bytes = 0;
        if( !stbi_write_png(oimagef, w, h, 1, (unsigned char*)oimage.data(), stride_in_bytes) ){
            printf("error writing png \'%s\'\n", oimagef);
        }
    }
//    for( int i = 0; i < w*h*3; i += 3 ){
//        int r,g,b;
//        r = data[i+0];
//        g = data[i+1];
//        b = data[i+2];
//        int old_value = (r<<16) | (g<<8) | b;
//        if( cmap.end() == cmap.find(old_value) ){ printf("cannot find class \'(%3d,%3d,%3d)\' in class map\n", r,g,b ); goto fail; }
//        int new_value = cmap[old_value];
//        data[i+0] = (new_value & (255<<16))>>16;
//        data[i+1] = (new_value & (255<< 8))>> 8;
//        data[i+2] = (new_value & (255<< 0))>> 0;
//        data[i+2] = cmap[old_value];
//    }
//    {
//        int stride_in_bytes = 0;
//        if( !stbi_write_png(oimagef, w, h, c, (unsigned char*)data, stride_in_bytes) ){
//            printf("error writing png \'%s\'\n", oimagef);
//        }
//    }

    free(data);

    //printf("c = %d %d %d\n", c, w, h);
    return 1;
fail:
    return 0;
}


int glance(char * filename ){
    int w, h, c;
    int nError = 0;
    printf("name: img= %s \n", filename);

    int channels = 0;
    unsigned char *data = stbi_load(filename, &w, &h, &c, channels);
    if (!data) {
        fprintf(stderr, "Cannot load image \"%s\"\nSTB Reason: %s\n", filename, stbi_failure_reason());
        exit(0);
    }
    int nPrinted = 0;
    printf("w = %8d h = %8d c = %8d\n", w, h, c);
    for( int i = 0; i < w*h*c && nPrinted < 50; i ++ ){
        printf("%5d", data[ random()%(w*h*c) ]), nPrinted ++ ;
    }
    printf("\n");

    free(data);

    //printf("c = %d %d %d\n", c, w, h);
    return 1;
fail:
    return 0;
}

int main( int argc, char ** argv ){
    if( argc < 2 ){
        printf("%10s: %-30s\n", "Usage", "./exe [option] [parameters]");
        printf("%10s %5s: %-30s\n", "[option]", "latrans", "label translator");
        return 0;
    }

    std::string option = argv[1];
    if( "latrans" == option ){
        if( argc < 1 + 5 ){
            printf("%18s: %-30s\n", "Usage", "latrans <image> <old class> <new class> <result>");
            printf("%18s: %-30s\n", "<image>", "input image");
            printf("%18s: %-30s\n", "<old class>", "R G B label");
            printf("%18s: %-30s\n", "<new class>", "label (sorted by new class id)");
            printf("%18s: %-30s\n", "<result>", "output image");
            return 1;
        }
        char * filename = argv[2+0];
        char * oclassf  = argv[2+1];
        char * nclassf  = argv[2+2];
        char * oimagef  = argv[2+3];
        if( !latrans( filename, oclassf, nclassf, oimagef ) )
            return 0;
    } else 
    if( "glance" == option ){
        if( argc < 1 + 2 ){
            printf("%18s: %-30s\n", "Usage", "glance <image>");
            printf("%18s: %-30s\n", "<image>", "input image");
            return 1;
        }
        char * filename = argv[2+0];
        if( !glance( filename ) )
            return 0;
    } else
    if( "nlsb0" == option ){
        if( argc < 1 + 4 ){
            printf("%18s: %-30s\n", "Usage", "<nlsb0> <nzero> <image> <result>");
            printf("%18s: %-30s\n", "<nlsb0>", "input image");
            printf("%18s: %-30s\n", "<image>", "input image");
            printf("%18s: %-30s\n", "<result>", "output image");
            return 1;
        }
        int nzero = atoi(argv[2+0]);
        char * filename = argv[2+1];
        char * oimagef  = argv[2+2];
        nlsb0(nzero, filename, oimagef);
    }

    return 1;
}
