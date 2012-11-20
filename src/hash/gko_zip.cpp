/*
 * gko_zip.cpp
 *
 *  Created on: Jan 11, 2012
 *      Author: auxten
 */


#include "lz4.h"
#include "gko_zip.h"

int gko_zip(char* source, char* dest, int isize)
{
    return LZ4_compress(source, dest, isize);
}

int gko_unzip(char* source, char* dest, int osize)
{
    return LZ4_uncompress(source, dest, osize);
}
