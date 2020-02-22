#ifndef CS222P_WINTER20_TYPES_H
#define CS222P_WINTER20_TYPES_H

typedef int RC;

/*******************************************************
                          PAGE
*******************************************************/

#define PAGE_SIZE 4096

typedef unsigned PageNum;
typedef unsigned short PageFreeSpace;   // depends on PAGE_SIZE
typedef unsigned short SlotNumber;      // number of slots in a page, depends on PAGE_SIZE
typedef unsigned PageOffset;          // depends on PageNum

typedef unsigned Counter;  // r/w/a counter

// pages in rbf, rm
#define PAGES_IN_FSP (PAGE_SIZE / sizeof(PageFreeSpace))
typedef unsigned short RecordNumber;    // depends on PAGE_SIZE
typedef char InitIndicator;
typedef bool SlotPointerIndicator;

// pages in ix
typedef SlotNumber KeyNumber;

/*******************************************************
                      ATTRIBUTE
*******************************************************/

typedef enum {
    TypeInt = 0, TypeReal, TypeVarChar, TypeNull
} AttrType;

typedef unsigned AttrLength;

struct Attribute {
    std::string name;  // attribute name
    AttrType type;     // attribute type
    AttrLength length; // attribute length
};

/*******************************************************
                        RECORD
*******************************************************/

// Record ID
typedef struct {
    unsigned pageNum;    // page number
    unsigned short slotNum;    // slot number in the page
} RID;

typedef unsigned short FieldNumber;   // type depends on PAGE_SIZE
typedef unsigned short FieldOffset;   // type depends on PAGE_SIZE
typedef unsigned short RecordVersion;
typedef unsigned short RecordSize;    // type depends on PAGE_SIZE

/*******************************************************
                          RM
*******************************************************/

# define SYSTABLE "Tables"
# define SYSCOLTABLE "Columns"
# define TUPLE_TMP_SIZE PAGE_SIZE

typedef unsigned TableID;


#endif //CS222P_WINTER20_TYPES_H
