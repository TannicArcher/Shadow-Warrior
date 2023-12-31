//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2005 - 3D Realms Entertainment

This file is NOT part of Shadow Warrior version 1.2
However, it is either an older version of a file that is, or is
some test code written during the development of Shadow Warrior.
This file is provided purely for educational interest.

Shadow Warrior is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Prepared for public release: 03/28/2005 - Charlie Wiederhold, 3D Realms
*/
//-------------------------------------------------------------------------

#include <fcntl.h>
#include <io.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <dos.h>
#include <malloc.h>
#include <stdlib.h>
#include "pragmas.h"

#pragma intrinsic(min);

#define MAXPALOOKUPS 256

static long numpalookups,transratio;
static char palettefilename[13];
static char palette[768], palookup[MAXPALOOKUPS<<8], transluc[65536];
static char closestcol[64][64][64];

#define FASTPALGRIDSIZ 8
static long rdist[129], gdist[129], bdist[129];
static char colhere[((FASTPALGRIDSIZ+2)*(FASTPALGRIDSIZ+2)*(FASTPALGRIDSIZ+2))>>3];
static char colhead[(FASTPALGRIDSIZ+2)*(FASTPALGRIDSIZ+2)*(FASTPALGRIDSIZ+2)];
static long colnext[256];
static char coldist[8] = {0,1,2,3,4,3,2,1};
static long colscan[27];

main(short int argc,char **argv)
{
    char col, ch;
    long fil, i, j, rscale, gscale, bscale;

    if ((argc != 3) && (argc != 6))
    {
        printf("\nSWTRANSPAL special version for Shadow Warrior, modified by Jim Norwood\n");
        printf("NOTE: You MUST run SWREMAP.EXE before running this!\n");
        printf("TRANSPAL [numshades][trans#(0-inv,256-opa)][r][g][b]     by Kenneth Silverman\n");
        printf("   Ex #1: transpal 32 170 30 59 11      (I use these values in my BUILD demo)\n");
        printf("                          ���������� The RGB scales are optional\n");
        printf("   Ex #2: transpal 64 160\n");
        exit(0);
    }

    strcpy(&palettefilename,"palette.dat");
    numpalookups = atol(argv[1]);
    transratio = atol(argv[2]);

    if (argc == 6)
    {
        rscale = atol(argv[3]);
        gscale = atol(argv[4]);
        bscale = atol(argv[5]);
    }
    else
    {
        rscale = 30;
        gscale = 59;
        bscale = 11;
    }

    if ((numpalookups < 1) || (numpalookups > 256))
        { printf("Invalid number of shades\n"); exit(0); }
    if ((transratio < 0) || (transratio > 256))
        { printf("Invalid transluscent ratio\n"); exit(0); }

    if ((fil = open(palettefilename,O_BINARY|O_RDWR,S_IREAD|S_IWRITE)) == -1)
    {
        printf("%s not found",palettefilename);
        exit(0);
    }
    read(fil,&palette[0],768);
    read(fil,&numpalookups,2);
    read(fil,&palookup[0],numpalookups<<8);

    setvmode(0x13);
    koutpw(0x3c4,0x0604);
    koutpw(0x3d4,0x0014);
    koutpw(0x3d4,0xe317);

    koutpw(0x3d4,9+(0<<8));

    koutpw(0x3c4,0x0f02);
    clearbuf(0xa0000,65536>>2,0L);

    koutp(0x3c8,0);
    for(i=0;i<768;i++) koutp(0x3c9,palette[i]);

    initfastcolorlookup(rscale,gscale,bscale);
    clearbuf(closestcol,262144>>2,0xffffffff);

    for(i=0;i<numpalookups;i++)
        for(j=0;j<256;j++)
        {
//          col = getpalookup((char)i,(char)j);
//          palookup[(i<<8)+j] = col;

            col = palookup[(i<<8)+j];
            outpw(0x3c4,2+(256<<((j+8)&3)));                //Draw top line
            drawpixel(((((i<<1)+0)*320+(j+8))>>2)+0xa0000,(long)col);
            drawpixel(((((i<<1)+1)*320+(j+8))>>2)+0xa0000,(long)col);
        }

    for(i=0;i<256;i++)
        for(j=0;j<6;j++)
        {
            koutpw(0x3c4,2+(256<<((i+8)&3)));                //Draw top line
            drawpixel((((j+132+0)*320+(i+8))>>2)+0xa0000,i);

            koutpw(0x3c4,2+(256<<((j+0)&3)));                //Draw left line
            drawpixel((((i+132+8)*320+(j+0))>>2)+0xa0000,i);
        }

    for(i=0;i<256;i++)
        for(j=0;j<256;j++)
        {
            col = gettrans((char)i,(char)j,transratio);
            transluc[(i<<8)+j] = col;

            koutpw(0x3c4,2+(256<<((i+8)&3)));
            drawpixel((((j+132+8)*320+(i+8))>>2)+0xa0000,(long)col);
        }

    do
    {
        ch = getch();
    } while ((ch != 32) && (ch != 13) && (ch != 27));

    if (ch == 13)
    {
        lseek(fil,768L,SEEK_SET);
        write(fil,&numpalookups,2);
        write(fil,palookup,numpalookups<<8);
        write(fil,transluc,65536);
    }
    else if (ch == 32)
    {
        lseek(fil,768L+2+(numpalookups<<8),SEEK_SET);
        write(fil,transluc,65536);
    }

    close(fil);

    setvmode(0x3);
}

getpalookup(char dashade, char dacol)
{
    long r, g, b, t;
    char *ptr;

    ptr = (char *)&palette[dacol*3];
    t = divscale16(numpalookups-dashade,numpalookups);
    r = ((ptr[0]*t+32768)>>16);
    g = ((ptr[1]*t+32768)>>16);
    b = ((ptr[2]*t+32768)>>16);
    return(getclosestcol(r,g,b));
}

gettrans(char dat1, char dat2, long datransratio)
{
    long r, g, b;
    char *ptr, *ptr2;

    ptr = (char *)&palette[dat1*3];
    ptr2 = (char *)&palette[dat2*3];
    r = ptr[0]; r += (((ptr2[0]-r)*datransratio+128)>>8);
    g = ptr[1]; g += (((ptr2[1]-g)*datransratio+128)>>8);
    b = ptr[2]; b += (((ptr2[2]-b)*datransratio+128)>>8);
    return(getclosestcol(r,g,b));
}

initfastcolorlookup(long rscale, long gscale, long bscale)
{
    long i, j, x, y, z;
    char *ptr;

    j = 0;
    for(i=64;i>=0;i--)
    {
        //j = (i-64)*(i-64);
        rdist[i] = rdist[128-i] = j*rscale;
        gdist[i] = gdist[128-i] = j*gscale;
        bdist[i] = bdist[128-i] = j*bscale;
        j += 129-(i<<1);
    }

    clearbufbyte(FP_OFF(colhere),sizeof(colhere),0L);
    clearbufbyte(FP_OFF(colhead),sizeof(colhead),0L);

    ptr = (char *)&palette[768-3];
    for(i=255;i>=0;i--,ptr-=3)
    {
        j = (ptr[0]>>3)*FASTPALGRIDSIZ*FASTPALGRIDSIZ+(ptr[1]>>3)*FASTPALGRIDSIZ+(ptr[2]>>3)+FASTPALGRIDSIZ*FASTPALGRIDSIZ+FASTPALGRIDSIZ+1;
        if (colhere[j>>3]&(1<<(j&7))) colnext[i] = colhead[j]; else colnext[i] = -1;
        colhead[j] = i;
        colhere[j>>3] |= (1<<(j&7));
    }

    i = 0;
    for(x=-FASTPALGRIDSIZ*FASTPALGRIDSIZ;x<=FASTPALGRIDSIZ*FASTPALGRIDSIZ;x+=FASTPALGRIDSIZ*FASTPALGRIDSIZ)
        for(y=-FASTPALGRIDSIZ;y<=FASTPALGRIDSIZ;y+=FASTPALGRIDSIZ)
            for(z=-1;z<=1;z++)
                colscan[i++] = x+y+z;
    i = colscan[13]; colscan[13] = colscan[26]; colscan[26] = i;
}

getclosestcol(long r, long g, long b)
{
    long i, j, k, dist, mindist, retcol;
    long *rlookup, *glookup, *blookup;
    char *ptr;

    if (closestcol[r][g][b] != 255) return(closestcol[r][g][b]);

    j = (r>>3)*FASTPALGRIDSIZ*FASTPALGRIDSIZ+(g>>3)*FASTPALGRIDSIZ+(b>>3)+FASTPALGRIDSIZ*FASTPALGRIDSIZ+FASTPALGRIDSIZ+1;
    mindist = min(rdist[coldist[r&7]+64+8],gdist[coldist[g&7]+64+8]);
    mindist = min(mindist,bdist[coldist[b&7]+64+8]);
    mindist++;

    rlookup = (long *)&rdist[64-r];
    glookup = (long *)&gdist[64-g];
    blookup = (long *)&bdist[64-b];

    retcol = -1;
    for(k=26;k>=0;k--)
    {
        i = colscan[k]+j; if ((colhere[i>>3]&(1<<(i&7))) == 0) continue;
        for(i=colhead[i];i>=0;i=colnext[i])
        {
            ptr = (char *)&palette[i*3];
            dist = glookup[ptr[1]]; if (dist >= mindist) continue;
            dist += rlookup[ptr[0]]; if (dist >= mindist) continue;
            dist += blookup[ptr[2]]; if (dist >= mindist) continue;
            mindist = dist; retcol = i;
        }
    }
    if (retcol < 0)
    {
        mindist = 0x7fffffff;
        ptr = (char *)&palette[768-3];
        for(i=255;i>=0;i--,ptr-=3)
        {
            dist = glookup[ptr[1]]; if (dist >= mindist) continue;
            dist += rlookup[ptr[0]]; if (dist >= mindist) continue;
            dist += blookup[ptr[2]]; if (dist >= mindist) continue;
            mindist = dist; retcol = i;
        }
    }
    ptr = (char *)&closestcol[r][g][b];
    *ptr = retcol;
    if ((r >= 4) && (ptr[(-2)<<12] == retcol)) ptr[(-3)<<12] = retcol, ptr[(-2)<<12] = retcol, ptr[(-1)<<12] = retcol;
    if ((g >= 4) && (ptr[(-2)<<6] == retcol)) ptr[(-3)<<6] = retcol, ptr[(-2)<<6] = retcol, ptr[(-1)<<6] = retcol;
    if ((b >= 4) && (ptr[(-2)] == retcol)) ptr[(-3)] = retcol, ptr[(-2)] = retcol, ptr[(-1)] = retcol;
    if ((r < 64-4) && (ptr[(2)<<12] == retcol)) ptr[(3)<<12] = retcol, ptr[(2)<<12] = retcol, ptr[(1)<<12] = retcol;
    if ((g < 64-4) && (ptr[(2)<<6] == retcol)) ptr[(3)<<6] = retcol, ptr[(2)<<6] = retcol, ptr[(1)<<6] = retcol;
    if ((b < 64-4) && (ptr[(2)] == retcol)) ptr[(3)] = retcol, ptr[(2)] = retcol, ptr[(1)] = retcol;
    if ((r >= 2) && (ptr[(-1)<<12] == retcol)) ptr[(-1)<<12] = retcol;
    if ((g >= 2) && (ptr[(-1)<<6] == retcol)) ptr[(-1)<<6] = retcol;
    if ((b >= 2) && (ptr[(-1)] == retcol)) ptr[(-1)] = retcol;
    if ((r < 64-2) && (ptr[(1)<<12] == retcol)) ptr[(1)<<12] = retcol;
    if ((g < 64-2) && (ptr[(1)<<6] == retcol)) ptr[(1)<<6] = retcol;
    if ((b < 64-2) && (ptr[(1)] == retcol)) ptr[(1)] = retcol;
    return(retcol);
}
