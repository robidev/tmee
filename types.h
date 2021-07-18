#ifndef types_h__
#define types_h__

#define BOOL        1  // item size = 1
#define INT8        2  // item size = 1
#define INT32       3  // item size = 4
#define INT64       4  // item size = 8
#define FLOAT32     5  // item size = 4
#define FLOAT64     6  // item size = 8
#define QUALITY     7  // item size = 4
#define MMSSTRING   8  // item size = max strlen
#define BITSTRING   9  // item size is then in bits instead of bytes
#define VISSTRING   10 // item size = max strlen
#define OCTSTRING   11 // item size = max strlen
#define GENTIME     12 // item size = 4?
#define BINTIME     13 // item size = 4?
#define UTCTIME     14 // item size = 4?

#define GOOSE       20 

#define SMV92       68 // item size = 64


#endif  // types_h__