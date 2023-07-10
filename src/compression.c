#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*=======*/
/* FFI */
/*=======*/

void *memset(void *, int, size_t);

void *malloc(size_t);
void free(void *);

typedef struct bytevec_s bytevec_t;

bytevec_t *bytevec_new(void);
void bytevec_free(bytevec_t *buf);
size_t bytevec_len(const bytevec_t *buf);
uint8_t bytevec_get(const bytevec_t *buf, size_t i);
void bytevec_push_back(bytevec_t *buf, uint8_t v);
uint8_t bytevec_pop_front(bytevec_t *buf);

/*=======*/
/* TYPES */
/*=======*/

typedef struct {
    uint8_t *write;
    uint8_t *writePos;
    size_t writeSize;
    const uint8_t *read;
    const uint8_t *readPos;
    size_t readSize;
} io_t;

typedef struct {
    int var0;
    int var1;
    int var2;
    int var3;
    io_t io;
} decoder_t;

typedef struct {
    int tableVar01[18];      // 800B2250
    short DecodeTable[2560]; // 800B22A8
    short array01[1258];     // 800B3660
    uint8_t BinaryTest[32];
} htables_t;

/*=========*/
/* GLOBALS */
/*=========*/

static const short ShiftTable[6] = {4, 6, 8, 10, 12, 14}; // 8005D8A0

/*
============================================================================

DECODE BASED ROUTINES

============================================================================
*/

/*
========================
=
= GetDecodeByte
=
========================
*/

static int GetDecodeByte(io_t *io) // 8002D1D0
{
    if ((size_t)(io->readPos - io->read) >= io->readSize)
        return -1;

    return *io->readPos++;
}

/*
========================
=
= WriteOutput
=
========================
*/

static int WriteOutput(io_t *io, uint8_t outByte) // 8002D214
{
    if ((size_t)(io->writePos - io->write) >= io->writeSize)
        return -1;

    *io->writePos++ = outByte;
    return 0;
}


/*
========================
=
= DecodeScan
=
========================
*/

static int DecodeScan(decoder_t *decoder) // 8002D2F4
{
    int resultbyte;

    resultbyte = decoder->var0;

    decoder->var0 = (resultbyte - 1);
    if ((resultbyte < 1))
    {
        resultbyte = GetDecodeByte(&decoder->io);
        if (resultbyte < 0)
            return -1;

        decoder->var1 = resultbyte;
        decoder->var0 = 7;
    }

    resultbyte = (0 < (decoder->var1 & 0x80));
    decoder->var1 = (decoder->var1 << 1);

    return resultbyte;
}


/*
========================
=
= RescanByte
=
========================
*/

static int RescanByte(decoder_t *decoder, int uint8_t) // 8002D3B8
{
    int shift;
    int i;
    int resultbyte;

    resultbyte = 0;
    i = 0;
    shift = 1;

    if(uint8_t <= 0)
        return resultbyte;

    do
    {
        int r = DecodeScan(decoder);
        if (r < 0)
            return -1;
        if (r != 0)
            resultbyte |= shift;

        i++;
        shift = (shift << 1);
    } while (i != uint8_t);

    return resultbyte;
}

/*
========================
=
= InitDecodeTable
=
========================
*/

static void InitDecodeTable(htables_t *tables) // 8002D468
{
    int evenVal, oddVal, incrVal;

    short *curArray;
    short *incrTbl;
    short *evenTbl;
    short *oddTbl;

    tables->tableVar01[15] = 3;
    tables->tableVar01[16] = 0;
    tables->tableVar01[17] = 0;

    curArray = &tables->array01[2];
    incrTbl = &tables->DecodeTable[0x4F2];

    incrVal = 2;

    do
    {
        if(incrVal < 0) {
            *incrTbl = (short)((incrVal + 1) >> 1);
        }
        else {
            *incrTbl = (short)(incrVal >> 1);
        }

        *curArray++ = 1;
        incrTbl++;
    } while(++incrVal < 1258);

    oddTbl  = &tables->DecodeTable[0x279];
    evenTbl = &tables->DecodeTable[1];

    evenVal = 2;
    oddVal = 3;

    do
    {
        *oddTbl++ = (short)oddVal;
        oddVal += 2;

        *evenTbl++ = (short)evenVal;
        evenVal += 2;

    } while(oddVal < 1259);

    tables->tableVar01[0] = 0;

    incrVal = (1 << ShiftTable[0]);
    tables->tableVar01[6] = (incrVal - 1);
    tables->tableVar01[1] = incrVal;

    incrVal += (1 << ShiftTable[1]);
    tables->tableVar01[7] = (incrVal - 1);
    tables->tableVar01[2] = incrVal;

    incrVal += (1 << ShiftTable[2]);
    tables->tableVar01[8] = (incrVal - 1);
    tables->tableVar01[3] = incrVal;

    incrVal += (1 << ShiftTable[3]);
    tables->tableVar01[9] = (incrVal - 1);
    tables->tableVar01[4] = incrVal;

    incrVal += (1 << ShiftTable[4]);
    tables->tableVar01[10] = (incrVal - 1);
    tables->tableVar01[5] = incrVal;

    incrVal += (1 << ShiftTable[5]);
    tables->tableVar01[11] = (incrVal - 1);
    tables->tableVar01[12] = (incrVal - 1);

    tables->tableVar01[13] = tables->tableVar01[12] + 64;

    memset(tables->BinaryTest, 0, sizeof tables->BinaryTest);
}

/*
========================
=
= CheckTable
=
========================
*/

static void CheckTable(htables_t *tables, int a0,int a1) // 8002D624
{
    int i;
    int idByte1;
    int idByte2;
    short *curArray;
    short *evenTbl;
    short *oddTbl;
    short *incrTbl;

    i = 0;
    evenTbl = &tables->DecodeTable[0];
    oddTbl  = &tables->DecodeTable[0x278];
    incrTbl = &tables->DecodeTable[0x4F0];

    idByte1 = a0;

    do {
        idByte2 = incrTbl[idByte1];

        tables->array01[idByte2] = (tables->array01[a1] + tables->array01[a0]);

        a0 = idByte2;

        if(idByte2 != 1) {
            idByte1 = incrTbl[idByte2];
            idByte2 = evenTbl[idByte1];

            a1 = idByte2;

            if(a0 == idByte2) {
                a1 = oddTbl[idByte1];
            }
        }

        idByte1 = a0;
    }while(a0 != 1);

    if(tables->array01[1] != 0x7D0) {
        return;
    }

    tables->array01[1] >>= 1;

    curArray = &tables->array01[2];
    do
    {
        curArray[3] >>= 1;
        curArray[2] >>= 1;
        curArray[1] >>= 1;
        curArray[0] >>= 1;
        curArray += 4;
        i += 4;
    } while(i != 1256);
}

/*
========================
=
= DecodeByte
=
========================
*/

static void DecodeByte(htables_t *tables, int tblpos) // 8002D72C
{
    int incrIdx;
    int evenVal;
    int idByte1;
    int idByte2;
    int idByte3;
    int idByte4;

    short *evenTbl;
    short *oddTbl;
    short *incrTbl;
    short *tmpIncrTbl;

    evenTbl = &tables->DecodeTable[0];
    oddTbl  = &tables->DecodeTable[0x278];
    incrTbl = &tables->DecodeTable[0x4F0];

    idByte1 = (tblpos + 0x275);
    tables->array01[idByte1] += 1;

    if (incrTbl[idByte1] != 1)
    {
        tmpIncrTbl = &incrTbl[idByte1];
        idByte2 = *tmpIncrTbl;

        if (idByte1 == evenTbl[idByte2]) {
            CheckTable(tables, idByte1, oddTbl[idByte2]);
        }
        else {
            CheckTable(tables, idByte1, evenTbl[idByte2]);
        }

        do
        {
            incrIdx = incrTbl[idByte2];
            evenVal = evenTbl[incrIdx];

            if (idByte2 == evenVal) {
                idByte3 = oddTbl[incrIdx];
            }
            else {
                idByte3 = evenVal;
            }

            if (tables->array01[idByte3] < tables->array01[idByte1])
            {
                if (idByte2 == evenVal) {
                    oddTbl[incrIdx] = (short)idByte1;
                }
                else {
                    evenTbl[incrIdx] = (short)idByte1;
                }

                evenVal = evenTbl[idByte2];

                if (idByte1 == evenVal) {
                    idByte4 = oddTbl[idByte2];
                    evenTbl[idByte2] = (short)idByte3;
                }
                else {
                    idByte4 = evenVal;
                    oddTbl[idByte2] = (short)idByte3;
                }

                incrTbl[idByte3] = (short)idByte2;

                *tmpIncrTbl = (short)incrIdx;
                CheckTable(tables, idByte3, idByte4);

                tmpIncrTbl = &incrTbl[idByte3];
            }

            idByte1 = *tmpIncrTbl;
            tmpIncrTbl = &incrTbl[idByte1];

            idByte2 = *tmpIncrTbl;
        } while (idByte2 != 1);
    }
}

/*
========================
=
= StartDecodeByte
=
========================
*/

static int StartDecodeByte(decoder_t *decoder, htables_t *tables) // 8002D904
{
    int lookup;
    short *evenTbl;
    short *oddTbl;

    lookup = 1;

    evenTbl = &tables->DecodeTable[0];
    oddTbl  = &tables->DecodeTable[0x278];

    while(lookup < 0x275)
    {
        int r = DecodeScan(decoder);
        if (r < 0)
            return INT_MIN;
        if (r == 0) {
            lookup = evenTbl[lookup];
        }
        else {
            lookup = oddTbl[lookup];
        }
    }

    lookup = (lookup + -0x275);
    DecodeByte(tables, lookup);

    return lookup;
}

/*
========================
=
= DecodeD64
=
= Exclusive Doom 64
=
========================
*/

ptrdiff_t DecodeD64(const uint8_t *input, size_t inputlen, uint8_t *output, size_t outputlen) // 8002DFA0
{
    int copyPos, storePos;
    int dec_byte, resc_byte;
    int incrBit, copyCnt, shiftPos, j;
    uint8_t *allocPtr;
    decoder_t decoder;
    htables_t tables;

    //PRINTF_D2(WHITE, 0, 15, "DecodeD64");

    InitDecodeTable(&tables);

    incrBit = 0;

    decoder.var0 = 0;
    decoder.var1 = 0;
    decoder.var2 = 0;
    decoder.var3 = 0;

    decoder.io.read = decoder.io.readPos = input;
    decoder.io.readSize = inputlen;
    decoder.io.write = decoder.io.writePos = output;
    decoder.io.writeSize = outputlen;

    allocPtr = malloc(tables.tableVar01[13]);

    dec_byte = StartDecodeByte(&decoder, &tables);
    if (dec_byte == INT_MIN)
        return -1;

    while(dec_byte != 256)
    {
        if(dec_byte < 256)
        {
            /* Decode the data directly using binary data code */

            if (WriteOutput(&decoder.io, (uint8_t)(dec_byte & 0xff)) < 0)
                goto fail;
            allocPtr[incrBit] = (uint8_t)dec_byte;

            /* Resets the count once the memory limit is exceeded in allocPtr,
               so to speak resets it at startup for reuse */
            incrBit += 1;
            if(incrBit == tables.tableVar01[13]) {
                incrBit = 0;
            }
        }
        else
        {
            /* Decode the data using binary data code,
               a count is obtained for the repeated data,
               positioning itself in the root that is being stored in allocPtr previously. */

            /*  A number is obtained from a range from 0 to 5,
                necessary to obtain a shift value in the ShiftTable*/
            shiftPos = (dec_byte + -257) / 62;

            /*  get a count number for data to copy */
            copyCnt  = (dec_byte - (shiftPos * 62)) + -254;

            /*  To start copying data, you receive a position number
                that you must sum with the position of table tableVar01 */
            resc_byte = RescanByte(&decoder, ShiftTable[shiftPos]);
            if (resc_byte < 0)
                goto fail;

            /*  with this formula the exact position is obtained
                to start copying previously stored data */
            copyPos = incrBit - ((tables.tableVar01[shiftPos] + resc_byte) + copyCnt);

            if(copyPos < 0) {
                copyPos += tables.tableVar01[13];
            }

            storePos = incrBit;

            for(j = 0; j < copyCnt; j++)
            {
                /* write the copied data */
                if (WriteOutput(&decoder.io, allocPtr[copyPos]) < 0)
                    goto fail;

                /* save copied data at current position in memory allocPtr */
                allocPtr[storePos] = allocPtr[copyPos];

                storePos++; /* advance to next allocPtr memory block to store */
                copyPos++;  /* advance to next allocPtr memory block to copy */

                /* reset the position of storePos once the memory limit is exceeded */
                if(storePos == tables.tableVar01[13]) {
                    storePos = 0;
                }

                /* reset the position of copyPos once the memory limit is exceeded */
                if(copyPos == tables.tableVar01[13]) {
                    copyPos = 0;
                }
            }

            /* Resets the count once the memory limit is exceeded in allocPtr,
               so to speak resets it at startup for reuse */
            incrBit += copyCnt;
            if (incrBit >= tables.tableVar01[13]) {
                incrBit -= tables.tableVar01[13];
            }
        }

        dec_byte = StartDecodeByte(&decoder, &tables);
        if (dec_byte == INT_MIN)
            goto fail;
    }

    free(allocPtr);

      //PRINTF_D2(WHITE, 0, 21, "DecodeD64:End");
    return decoder.io.writePos - decoder.io.write;

fail:
    free(allocPtr);
    return -1;
}

static int MakeByte(bytevec_t *BinCode, io_t *io)
{
    //for (int BinCnt = 0; BinCnt < BinCode.size();)

    //printf("Make File Output\n");

    while(1)
    {
        if(!bytevec_len(BinCode)){break;}
        if(bytevec_len(BinCode) < 8){break;}

        int i = 0;              // $s1
        int shift = 1;          // $s0
        int resultbyte = 0;     // $s2
        int binary = 0;

        do
        {
            binary = bytevec_get(BinCode, 7 - i);

            if(!(binary == 0)) {
                resultbyte |= shift;
            }

            i++;
            shift = (shift << 1);
            //printf("binary %x\n",binary);
        }
        while(i != 8);

        for(binary = 0; binary < i; binary++)//Remove first 8 bytes
        {
            bytevec_pop_front(BinCode);
        }

        //printf("resultbyte shift %x\n",resultbyte);

        if (WriteOutput(io, resultbyte) < 0)
            return -1;
        //getch();
    }
    return 0;
}

//Count Table
static const int CountTable[64][6] = {
    { 0x0000, 0x0010, 0x0050, 0x0150, 0x0550, 0x1550 },
    { 0x0001, 0x0011, 0x0051, 0x0151, 0x0551, 0x1551 },
    { 0x0002, 0x0012, 0x0052, 0x0152, 0x0552, 0x1552 },
    { 0x0003, 0x0013, 0x0053, 0x0153, 0x0553, 0x1553 },
    { 0x0004, 0x0014, 0x0054, 0x0154, 0x0554, 0x1554 },
    { 0x0005, 0x0015, 0x0055, 0x0155, 0x0555, 0x1555 },
    { 0x0006, 0x0016, 0x0056, 0x0156, 0x0556, 0x1556 },
    { 0x0007, 0x0017, 0x0057, 0x0157, 0x0557, 0x1557 },
    { 0x0008, 0x0018, 0x0058, 0x0158, 0x0558, 0x1558 },
    { 0x0009, 0x0019, 0x0059, 0x0159, 0x0559, 0x1559 },
    { 0x000a, 0x001a, 0x005a, 0x015a, 0x055a, 0x155a },
    { 0x000b, 0x001b, 0x005b, 0x015b, 0x055b, 0x155b },
    { 0x000c, 0x001c, 0x005c, 0x015c, 0x055c, 0x155c },
    { 0x000d, 0x001d, 0x005d, 0x015d, 0x055d, 0x155d },
    { 0x000e, 0x001e, 0x005e, 0x015e, 0x055e, 0x155e },
    { 0x000f, 0x001f, 0x005f, 0x015f, 0x055f, 0x155f },
    { 0x0010, 0x0020, 0x0060, 0x0160, 0x0560, 0x1560 },
    { 0x0011, 0x0021, 0x0061, 0x0161, 0x0561, 0x1561 },
    { 0x0012, 0x0022, 0x0062, 0x0162, 0x0562, 0x1562 },
    { 0x0013, 0x0023, 0x0063, 0x0163, 0x0563, 0x1563 },
    { 0x0014, 0x0024, 0x0064, 0x0164, 0x0564, 0x1564 },
    { 0x0015, 0x0025, 0x0065, 0x0165, 0x0565, 0x1565 },
    { 0x0016, 0x0026, 0x0066, 0x0166, 0x0566, 0x1566 },
    { 0x0017, 0x0027, 0x0067, 0x0167, 0x0567, 0x1567 },
    { 0x0018, 0x0028, 0x0068, 0x0168, 0x0568, 0x1568 },
    { 0x0019, 0x0029, 0x0069, 0x0169, 0x0569, 0x1569 },
    { 0x001a, 0x002a, 0x006a, 0x016a, 0x056a, 0x156a },
    { 0x001b, 0x002b, 0x006b, 0x016b, 0x056b, 0x156b },
    { 0x001c, 0x002c, 0x006c, 0x016c, 0x056c, 0x156c },
    { 0x001d, 0x002d, 0x006d, 0x016d, 0x056d, 0x156d },
    { 0x001e, 0x002e, 0x006e, 0x016e, 0x056e, 0x156e },
    { 0x001f, 0x002f, 0x006f, 0x016f, 0x056f, 0x156f },
    { 0x0020, 0x0030, 0x0070, 0x0170, 0x0570, 0x1570 },
    { 0x0021, 0x0031, 0x0071, 0x0171, 0x0571, 0x1571 },
    { 0x0022, 0x0032, 0x0072, 0x0172, 0x0572, 0x1572 },
    { 0x0023, 0x0033, 0x0073, 0x0173, 0x0573, 0x1573 },
    { 0x0024, 0x0034, 0x0074, 0x0174, 0x0574, 0x1574 },
    { 0x0025, 0x0035, 0x0075, 0x0175, 0x0575, 0x1575 },
    { 0x0026, 0x0036, 0x0076, 0x0176, 0x0576, 0x1576 },
    { 0x0027, 0x0037, 0x0077, 0x0177, 0x0577, 0x1577 },
    { 0x0028, 0x0038, 0x0078, 0x0178, 0x0578, 0x1578 },
    { 0x0029, 0x0039, 0x0079, 0x0179, 0x0579, 0x1579 },
    { 0x002a, 0x003a, 0x007a, 0x017a, 0x057a, 0x157a },
    { 0x002b, 0x003b, 0x007b, 0x017b, 0x057b, 0x157b },
    { 0x002c, 0x003c, 0x007c, 0x017c, 0x057c, 0x157c },
    { 0x002d, 0x003d, 0x007d, 0x017d, 0x057d, 0x157d },
    { 0x002e, 0x003e, 0x007e, 0x017e, 0x057e, 0x157e },
    { 0x002f, 0x003f, 0x007f, 0x017f, 0x057f, 0x157f },
    { 0x0030, 0x0040, 0x0080, 0x0180, 0x0580, 0x1580 },
    { 0x0031, 0x0041, 0x0081, 0x0181, 0x0581, 0x1581 },
    { 0x0032, 0x0042, 0x0082, 0x0182, 0x0582, 0x1582 },
    { 0x0033, 0x0043, 0x0083, 0x0183, 0x0583, 0x1583 },
    { 0x0034, 0x0044, 0x0084, 0x0184, 0x0584, 0x1584 },
    { 0x0035, 0x0045, 0x0085, 0x0185, 0x0585, 0x1585 },
    { 0x0036, 0x0046, 0x0086, 0x0186, 0x0586, 0x1586 },
    { 0x0037, 0x0047, 0x0087, 0x0187, 0x0587, 0x1587 },
    { 0x0038, 0x0048, 0x0088, 0x0188, 0x0588, 0x1588 },
    { 0x0039, 0x0049, 0x0089, 0x0189, 0x0589, 0x1589 },
    { 0x003a, 0x004a, 0x008a, 0x018a, 0x058a, 0x158a },
    { 0x003b, 0x004b, 0x008b, 0x018b, 0x058b, 0x158b },
    { 0x003c, 0x004c, 0x008c, 0x018c, 0x058c, 0x158c },
    { 0x003d, 0x004d, 0x008d, 0x018d, 0x058d, 0x158d },
    { 0x003e, 0x004e, 0x008e, 0x018e, 0x058e, 0x158e },
    { 0x003f, 0x004f, 0x008f, 0x018f, 0x058f, 0x158f }
};

/*
void InitCountTable()
{
     int i;

     for(i = 0; i <= 0x40; i++)
     {
           CountTable[i][0] = 0 + i;
           CountTable[i][1] = 16 + i;
           CountTable[i][2] = 80 + i;
           CountTable[i][3] = 336 + i;
           CountTable[i][4] = 1360 + i;
           CountTable[i][5] = 5456 + i;
     }
}
*/


static void MakeBinary(bytevec_t *BinCode, htables_t *tables, int lookup, bool save)
{
     short *tablePtr1 = tables->DecodeTable;           // $s2
     short *tablePtr2 = &tables->DecodeTable[0x278];   // $s1

     int Code = lookup;
     int Cnt = 0;
     uint8_t Binary[632] = {0,};

     while (1)
     {
         if(lookup <= 1){break;}

         for(int i = 1; i < 632; i++)
         {
              int lookupcheck = tablePtr1[i];

              if(lookupcheck == lookup)
              {
                   //scan = 0;
                   //if(lookup == 0x0375) getch();
                   //printf("is 0 %x, poss %x, cnt %d\n", lookup, i, Cnt+1);
                   Binary[Cnt] = 0;
                   lookup = i;
                   Cnt++;
                   break;
              }
         }


         for(int i = 1; i < 632; i++)
         {
              int lookupcheck = tablePtr2[i];

              if(lookupcheck == lookup)
              {
                   //scan = 1;
                   //if(lookup == 0x0375) getch();
                   //printf("is 1 %x, poss %x, cnt %d\n", lookup, i, Cnt+1);
                   Binary[Cnt] = 1;
                   lookup = i;
                   Cnt++;
                   break;
              }
         }
     }

     if(save)
     {
         //Copy Binary
         for(int j = 0; j < Cnt; j++)
         {
              //printf("%d\n",Binary[(Cnt-1)-j]);
              bytevec_push_back(BinCode, Binary[(Cnt-1)-j]);
              //BinCode.push_back(Binary[j]);
         }

         lookup = (Code + (signed short)0xFD8B);
         //printf("lookup %X\n",lookup);
         DecodeByte(tables, lookup);
     }
     else
     {
         //Copy Binary Test
         for(int j = 0; j < Cnt; j++)
         {
              tables->BinaryTest[j] = Binary[(Cnt-1)-j];
         }

     }

     if(Code == 0x0375)
     {
         //getch();
         int pow = (bytevec_len(BinCode)) % 8;
         //printf("size %d pow4 %d\n",(BinCode.size()), pow);
         if(pow != 0)
         {
            //printf("Add\n");
            for (int i = 0 ; i < (8 - pow); i++)
            {
              bytevec_push_back(BinCode, false);
            }
         }
     }

     //if(Cnt > 16)
        // getch();
}

static void MakeExtraBinary(bytevec_t *BinCode, int Value, int Shift)
{
     unsigned short pixel = Value;

     //setcolor2(0x03);
     for(int b = 0; b < Shift; b++)
     {
          if((pixel & 1 << b) != 0)
          {
               //printf("1");
               bytevec_push_back(BinCode, true);
          }
          else
          {
               //printf("0");
               bytevec_push_back(BinCode, false);//
          }

          //if(b > 6 && b < 8) printf(" ");
     }

     //Copy Binary
     /*for(int j = 0; j < Shift; j++)
     {
          //printf("%d\n",Binary[(Cnt-1)-j]);
          BinCode.push_back(binario[j]);
          //BinCode.push_back(Binary[j]);
     }*/

     //printf(" %x\n",pixel);//getch();
     //setcolor2(0x07);
}

ptrdiff_t EncodeD64(const uint8_t *input, size_t inputlen, uint8_t *output, size_t outputlen)
{
    int v[2];
    int a[4];
    int s[10];
    int t[10];
    int at;
    int div;
    int mul;
    io_t io;
    htables_t tables;
    bytevec_t *BinCode;
    uint8_t *allocPtr;

    io.writePos = io.write = output;
    io.writeSize = outputlen;

    InitDecodeTable(&tables);

    allocPtr = malloc(tables.tableVar01[13]);

    int lookup = 1;                                 // $s0
    short *tablePtr1 = tables.DecodeTable;                  // $s2
    short *tablePtr2 = &tables.DecodeTable[0x278];   // $s1

    uint8_t *s4p;
    uint8_t *t1p;
    uint8_t *t2p;
    uint8_t *t4p;
    uint8_t *t8p;
    uint8_t *t9p;
    uint8_t *v0p;
    s4p = (uint8_t*)allocPtr;
    int incrBit;
    size_t incrBitFile;
    int offset;
    bool copy;
    bool make;
    int j,k,l,m, bin;
    size_t i;

    int Max = 0x558f;
    int LooKupCode = 0;

    incrBitFile = 0;
    incrBit = 0;
    bin = 0;
    //Paso 1 Copy 14 Bytes

    BinCode = bytevec_new();

    for(i = 0; i < 14; i++)
    {
        if(incrBitFile > inputlen)
            goto fail;

        t8p = s4p;
        t9p = (t8p + incrBit);
        *t9p = input[incrBitFile];

        //Make Binary
        LooKupCode = (input[incrBitFile] + 0x0275);
        MakeBinary(BinCode, &tables, LooKupCode, true);
        if (MakeByte(BinCode, &io) < 0)
            goto fail;

        incrBit++;
        incrBitFile++;
    }


    while(1)
    {
        if(incrBitFile > inputlen) break;

        //printf("Compress (%%%.2f)\n", prc*100);

        offset = 0;
        copy = false;

        //int pow2 = (BinCode.size()) % 8;
        //if(pow2 != 0)
        for(j = 64; j >= 3; j--)
        {
            if(copy) break;

            int minval = incrBit - 1024;
            for(k = incrBit; k >= minval; k--)
            {
                if(copy) break;
                for(l = 0; l < j; l++)
                {
                    offset = (k - j) + l;

                    if(offset < 0)
                    {
                        //offset += Max;
                        //continue;
                        break;
                    }

                    //printf("offset %d poss = %d\n", offset, incrBitFile + l);
                    //printf("A = %X || B = %X\n", s4p[offset], input[incrBitFile + l]);
                    if(incrBitFile + l > inputlen)
                        goto fail;

                    if(s4p[offset] != input[incrBitFile + l])
                    {
                        copy = false;
                        break;
                    }
                    else
                    {
                        copy = true;
                    }
                    //getch();
                }
            }
        }

        if(copy)
        {
            //printf("\nCopy\n");
            //printf("offset = %d || offset1 = %d || count %d\n", offset-j, incrBit, j+1);
            int rest = (incrBit - (offset-j));
            int count = (j+1);
            //printf("rest = %d\n", rest);

            //Make Count Code
            int ShiftVal[6] = {0x0f, 0x3F, 0xFF, 0x3FF, 0xFFF, 0x3FFF};
            int Shift = 0x04;
            for(m = 0; m < 6; m++)
            {
                //printf("Count = %d -> %d ", count, CountTable[count][m]);
                int maxval = CountTable[count][m] + ShiftVal[m];
                //printf("Max %d  Shift %X", maxval, Shift);
                if(rest <= maxval) {/*printf("\n");*/break; /*printf("This");*/}
                Shift += 2;
            }
            //printf("\n");

            int ValExtra = (rest - CountTable[count][m > 5 ? 5 : m]);
            //printf("ValExtra = %d\n", ValExtra);

            if(Shift == 0x04){LooKupCode = (0x0376 + (count - 3));}
            if(Shift == 0x06){LooKupCode = (0x03B4 + (count - 3));}
            if(Shift == 0x08){LooKupCode = (0x03F2 + (count - 3));}
            if(Shift == 0x0A){LooKupCode = (0x0430 + (count - 3));}
            if(Shift == 0x0C){LooKupCode = (0x046E + (count - 3));}
            if(Shift == 0x0E){LooKupCode = (0x04AC + (count - 3));}

            //printf("Code 0x%04X ValExtra %d\n", LooKupCode, ValExtra);

            //comprobando

            //BinaryTest
            MakeBinary(BinCode, &tables, LooKupCode, false);

            bin = 0;
            lookup = 1;                                 // $s0
            while(lookup < 0x275) {
                if(tables.BinaryTest[bin] == 0) {
                    lookup = tablePtr1[lookup];
                    //printf("lookup1 %X\n",lookup);
                }
                else {
                    lookup = tablePtr2[lookup];
                    //printf("lookup2 %X\n",lookup);
                }
                bin++;
            }
            //printf("lookup %X\n",lookup);
            //getch();

            //lookup = (lookup + (signed short)0xFD8B);

            //s[0] = (LooKupCode + (signed short)0xFD8B);
            s[0] = (lookup + (signed short)0xFD8B);
            v[0] = 62;

            //s[0] = 256;
            t[2] = (s[0] + (signed short)0xFEFF);
            //printf("s[0] = %d\n",s[0]);
            //printf("t[2] = %d\n",t[2]);

            div = t[2] / v[0];

            at = -1;

            // GhostlyDeath <May 15, 2010> -- loc_8002E0C4 is an if
            if(v[0] == at) {
                at = 0x8000;
            }

            s[2] = 0;
            s[5] = div;
            //printf("s[5] = %d\n",s[5]);

            mul = s[5] * v[0];

            a[0] = ShiftTable[s[5]];
            //printf("a[0] = %X t[4] = %X\n",a[0], t[4]);

            t[3] = mul;
            //printf("t[3] = %d\n",t[3]);

            s[8] = (s[0] - t[3]);       // subu    $fp, $s0, $t3
                                        //printf("s[8] = %d\n",s[8]);
            s[8] += (signed short)0xFF02;       // addiu   $fp, 0xFF02
                                                //printf("Count s[8] = %d\n",s[8]);
            s[3] = s[8];//Count to copy                // move    $s3, $fp

            //printf("shift a[0] = %d\n",a[0]);
            v[0] = ValExtra;//Deflate_RescanByte(a[0]);
                            //printf("v[0] = %d\n",v[0]);

                            //printf("t[5] = %d\n",t[5]);
            t[6] = tables.tableVar01[s[5]];
            //printf("t[6] = %d\n",t[6]);
            s[1] = incrBit;

            t[7] = (t[6] + v[0]);
            //printf("t[7] = %d\n",t[7]);
            v[1] = (t[7] + s[8]);                   // addu $v1, $t7, $fp
                                                    //printf("Rest v[1] = %d\n",v[1]);//valor a restar
            a[0] = (incrBit - v[1]);                // subu input, incrBit, $v1
                                                    //printf("incrBit %d, a[0] = %d\n",incrBit, a[0]);
            s[0] = a[0];                            // move $s0, input

            // GhostlyDeath <May 15, 2010> -- loc_8002E124 is an if
            if(a[0] < 0) {                      // bgez input, loc_8002E124
                t[8] = Max;
                s[0] = (a[0] + t[8]);
            }

            //printf("s[0] = %d\n",s[0]);

            // GhostlyDeath <May 15, 2010> -- loc_8002E184 is an if

            make = false;
            l = 0;
            //if(lookup == LooKupCode)
            if(s[8] > 0)
            {
                //printf("s[8] = %X\n",s[8]);
                // GhostlyDeath <May 15, 2010> -- loc_8002E12C is a while loop (jump back from end)
                while(s[2] != s[3])
                {
                    //printf("s[2] = %X\n",s[2]);
                    //printf("s[3] = %X\n",s[3]);
                    t9p = s4p;
                    t1p = (t9p + s[0]);
                    a[0] = *(uint8_t*)t1p;             // lbu  input, 0($t1)

                    if(incrBitFile + l > inputlen)
                        goto fail;

                    if(a[0] == input[incrBitFile + l]) {make = true;}
                    else {make = false; break;}
                    //printf("a[0] = %X\n",a[0]);
                    //setcolor2(0x0B);//printf("%02X",a[0]);getch();
                    //printf("out a[0] = %X (%d , %d)\n",a[0],  s[0], incrBit+s[2]);//getch();

                    v0p = s4p;
                    s[2] += 1;

                    t2p = (v0p + s[0]);                 // addu $t2, $s0, $v0
                    t[3] = *(uint8_t*)t2p;

                    t4p = (v0p + s[1]);
                    *(uint8_t*)t4p = t[3];

                    v[1] = Max;

                    //printf("s[0] = %d || s[1] = %d\n",s[0],s[1]);

                    s[1]++;
                    s[0]++;

                    // GhostlyDeath <May 15, 2010> -- loc_8002E170 is an if
                    if(s[1] == v[1]) {
                        s[1] = 0;
                    }

                    // GhostlyDeath <May 15, 2010> -- loc_8002E17C is an if
                    if(s[0] == v[1]) {
                        s[0] = 0;
                    }

                    l++;
                }
            }

            if(make)
            {
                MakeBinary(BinCode, &tables, LooKupCode, true);
                if (MakeByte(BinCode, &io) < 0)
                    goto fail;
                MakeExtraBinary(BinCode, ValExtra, Shift);
                if (MakeByte(BinCode, &io) < 0)
                    goto fail;

                for(i = 0; i < (size_t)(j+1); i++)
                {
                    if(incrBitFile > inputlen)
                        goto fail;

                    t8p = s4p;
                    t9p = (t8p + incrBit);
                    *t9p = input[incrBitFile];
                    incrBit++;
                    incrBitFile++;
                }

                at = (incrBit < Max);
                if(at == 0)
                {
                    incrBit -= Max;
                }
                //getch();
            }
            else
            {
                //setcolor2(0x04);printf("No save\n");
                t8p = s4p;
                t9p = (t8p + incrBit);
                *t9p = input[incrBitFile];

                //Make Binary
                LooKupCode = (input[incrBitFile] + 0x0275);
                //setcolor2(0x0A);printf("Code 0x%04X -> 0x%02X\n", LooKupCode, input[incrBitFile]);setcolor2(0x07);
                MakeBinary(BinCode, &tables, LooKupCode, true);
                if (MakeByte(BinCode, &io) < 0)
                    goto fail;

                incrBit++;
                incrBitFile++;
                if(incrBit == Max)
                {
                    incrBit = 0;
                }
                //getch();
            }

            //getch();
        }
        else
        {
            t8p = s4p;
            t9p = (t8p + incrBit);
            *t9p = input[incrBitFile];

            //Make Binary
            LooKupCode = (input[incrBitFile] + 0x0275);
            //setcolor2(0x0A);printf("Code 0x%04X -> 0x%02X\n", LooKupCode, input[incrBitFile]);setcolor2(0x07);
            MakeBinary(BinCode, &tables, LooKupCode, true);
            if (MakeByte(BinCode, &io) < 0)
                goto fail;

            incrBit++;
            incrBitFile++;
            if(incrBit == Max)
            {
                incrBit = 0;
            }
            //printf("\n");
            //getch();
        }
    }

    MakeBinary(BinCode, &tables, 0x0375, true);
    if (MakeByte(BinCode, &io) < 0)
        goto fail;

    size_t Align4 = (io.writePos - io.write) % 4;
    if(Align4 != 0)
    {
        //printf("Add\n");
        for (i = 0 ; i < (4 - Align4); i++)
        {
            //*output++ = 0;
            int val = 0x00;
            if (WriteOutput(&io, val) < 0)
                goto fail;
        }
    }

    free(allocPtr);
    return io.writePos - io.write;
    /* TEST
    FILE *f3 = fopen ("Alloc2.bin","wb");
    for(i = 0; i < Max; i++)
    {
    fwrite (&s4p[i],sizeof(uint8_t),1,f3);
    }
    fclose(f3);
    */
fail:
    free(allocPtr);
    return -1;
}

/*
== == == == == == == == == ==
=
= DecodeJaguar (decode original name)
=
= Exclusive Psx Doom / Doom 64 from Jaguar Doom
=
== == == == == == == == == ==
*/

#define WINDOW_SIZE      4096

#define LENSHIFT 4            /* this must be log2(LOOKAHEAD_SIZE) */
#define LOOKAHEAD_SIZE      (1<<LENSHIFT)

void DecodeJaguar(const uint8_t *input, uint8_t *output) // 8002E1f4
{
    int getidbyte = 0;
    size_t len;
    size_t pos;
    size_t i;
    uint8_t *source;
    int idbyte = 0;

    while (1)
    {
        /* get a new idbyte if necessary */
        if (!getidbyte) idbyte = *input++;
        getidbyte = (getidbyte + 1) & 7;

        if (idbyte & 1)
        {
            /* decompress */
            pos = *input++ << LENSHIFT;
            pos = pos | (*input >> LENSHIFT);
            source = output - pos - 1;
            len = (*input++ & 0xf) + 1;
            if (len == 1) break;

            //for (i = 0; i<len; i++)
            //*output++ = *source++;

            i = 0;
            if (len > 0)
            {
                if ((len & 3))
                {
                    while(i != (len & 3))
                    {
                        *output++ = *source++;
                        i++;
                    }
                }
                while(i != len)
                {
                    output[0] = source[0];
                    output[1] = source[1];
                    output[2] = source[2];
                    output[3] = source[3];
                    output += 4;
                    source += 4;
                    i += 4;
                }
            }
        }
        else
        {
            *output++ = *input++;
        }

        idbyte = idbyte >> 1;
    }
}

typedef struct node_struct node_t;

///////////////////////////////////////////////////////////////////////////
//    IMPORTANT: FOLLOWING STRUCTURE MUST BE 16 OR 32 BYTES IN LENGTH    //
///////////////////////////////////////////////////////////////////////////

struct node_struct
{
    const uint8_t *pointer;
    node_t        *prev;
    node_t        *next;
    intptr_t       pad;
};

typedef struct
{
    node_t *start;
    node_t *end;
} list_t;

typedef struct {
    list_t hashtable[256]; // used for faster encoding
    node_t hashtarget[WINDOW_SIZE]; // what the hash points to
} lzss_encoder_t;

static void addnode(lzss_encoder_t *encoder, const uint8_t *pointer)
{

    list_t *list;
    intptr_t targetindex;
    node_t *target;

    targetindex = (intptr_t) pointer & ( WINDOW_SIZE - 1 );

    // remove the target node at this index

    target = &encoder->hashtarget[targetindex];
    if (target->pointer)
    {
        list = &encoder->hashtable[*target->pointer];
        if (target->prev)
        {
            list->end = target->prev;
            target->prev->next = 0;
        }
        else
        {
            list->end = 0;
            list->start = 0;
        }
    }

    // add a new node to the start of the hashtable list

    list = &encoder->hashtable[*pointer];

    target->pointer = pointer;
    target->prev = 0;
    target->next = list->start;
    if (list->start)
        list->start->prev = target;
    else
        list->end = target;
    list->start = target;

}

ptrdiff_t EncodeJaguar(const uint8_t *input, size_t inputlen, uint8_t *output, size_t outputlen)
{

    int putidbyte = 0;
    const uint8_t *encodedpos = input;
    size_t encodedlen;
    size_t i;
    //ptrdiff_t pacifier=0;
    size_t len;
    const uint8_t *window;
    const uint8_t *lookahead;
    uint8_t *idbyte;
    node_t *hashp;
    size_t lookaheadlen;
    size_t samelen;
    io_t io;

    io.write = io.writePos = output;
    io.writeSize = outputlen;

    // check the output
    if (outputlen < (inputlen * 9)/8+1)
        return -1;

    lzss_encoder_t *encoder = malloc(sizeof *encoder);

    memset(encoder, 0, sizeof *encoder);

    // initialize the window & lookahead
    lookahead = window = input;

    while (inputlen > 0)
    {

        // set the window position and size
        window = lookahead - WINDOW_SIZE;
        if (window < input) window = input;

        // decide whether to allocate a new id uint8_t
        if (!putidbyte)
        {
            idbyte = io.writePos;
            if (WriteOutput(&io, 0) < 0)
                goto fail;
        }
        putidbyte = (putidbyte + 1) & 7;

        // go through the hash table of linked lists to find the strings
        // starting with the first character in the lookahead

        encodedlen = 0;
        lookaheadlen = inputlen < LOOKAHEAD_SIZE ? inputlen : LOOKAHEAD_SIZE;

        hashp = encoder->hashtable[lookahead[0]].start;
        while (hashp)
        {

            samelen = 0;
            len = lookaheadlen;
            while (len-- && hashp->pointer[samelen] == lookahead[samelen])
                samelen++;
            if (samelen > encodedlen)
            {
                encodedlen = samelen;
                encodedpos = hashp->pointer;
            }
            if (samelen == lookaheadlen) break;

            hashp = hashp->next;
        }

        // encode the match and specify the length of the encoding
        if (encodedlen >= 3)
        {
            *idbyte = (*idbyte >> 1) | 0x80;
            if (WriteOutput(&io, (lookahead-encodedpos-1) >> LENSHIFT) < 0)
                goto fail;
            if (WriteOutput(&io, ((lookahead-encodedpos-1) << LENSHIFT) | (encodedlen-1)) <0)
                goto fail;
        } else { // or just store the unmatched uint8_t
            encodedlen = 1;
            *idbyte = (*idbyte >> 1);
            if (WriteOutput(&io, *lookahead) < 0)
                goto fail;
        }

        // update the hash table as the window slides
        for (i=0 ; i<encodedlen ; i++)
            addnode(encoder, lookahead++);

        // reduce the input size
        if (encodedlen > inputlen)
            goto fail;
        else
            inputlen -= encodedlen;

        /*
        // print pacifier dots
        pacifier -= encodedlen;
        if (pacifier<=0)
        {
            fprintf(stdout, ".");
            pacifier += 10000;
        }
        */

    }

    // put the end marker on the file
    if (!putidbyte)
    {
        if (WriteOutput(&io, 1) < 0)
            goto fail;
    }
    else
        *idbyte = ((*idbyte>>1)|0x80)>>(7-putidbyte);

    if (WriteOutput(&io, 0) < 0)
        goto fail;
    if (WriteOutput(&io, 0) < 0)
        goto fail;

    /*
       fprintf(stdout, "\nnum bytes = %d\n", numbytes);
       fprintf(stdout, "num codes = %d\n", numcodes);
       fprintf(stdout, "ave code length = %f\n", (double) codelencount/numcodes);
       fprintf(stdout, "size = %d\n", *size);
       */

    free(encoder);
    return io.writePos - io.write;

fail:
    free(encoder);
    return -1;
}

