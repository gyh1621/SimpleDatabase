#include <cstring>
#include <cmath>
#include <vector>
#include <cassert>
#include <iostream>

#ifndef CS222_FALL19_RECORD_H
#define CS222_FALL19_RECORD_H

// Record ID
typedef struct {
    unsigned pageNum;    // page number
    unsigned short slotNum;    // slot number in the page
} RID;

// Attribute
typedef enum {
    TypeInt = 0, TypeReal, TypeVarChar
} AttrType;

typedef unsigned AttrLength;

struct Attribute {
    std::string name;  // attribute name
    AttrType type;     // attribute type
    AttrLength length; // attribute length
};

typedef unsigned short FieldNumber;   // type depends on PAGE_SIZE
typedef unsigned short FieldOffset;   // type depends on PAGE_SIZE


class Record {

    /*
     * Record Format:
     * ┌────────────────┬──────────────────────┬─────────────────────────┐
     * │  RecordHeader  │     OffsetSection    │        FieldData        │
     * ├────────────────┼──────────────────────┼─────────────────────────┤
     * │  Field Number  │  offset, ..., offset │ Field 1 | ... | Field N │
     * │ unsigned short │    unsigned short    │ ......................  │
     * └────────────────┴──────────────────────┴─────────────────────────┘
     *
     * Note: 1. null field will not occupy a "Field" data space
     *          but will still occupy a "field offset" space and the offset is '0'
     *       2. varchar's length will be stored as 2 bytes data, so current maximum length of varchar is 65536
     *       3. offset points to end of the data
     */

    static const unsigned short recordHeaderSize = sizeof(unsigned short);

    /* Get a record's actual size in bytes from raw record data */
    static int getRecordActualSize(const int &nullIndicatorSize, const std::vector<Attribute> &recordDescriptor, const void *data);

    /* Get null indicator data's size by field number */
    static int getNullIndicatorSize(const int &fieldNumber);

    /* Examine a field is null or not, *data is raw record data
     * Note: assume all arguments are valid.
     * */
    static bool isFieldNull(const int &fieldIndex, const void *nullIndicatorData);

private:
    int size;
    void *record;
    int offsetSectionOffset;  // offset to the start of offset section
    FieldNumber fieldNumber;
    bool passedData; // indicate whether record space is passed in or created in the class
public:
    Record(void* data);
    Record(const std::vector<Attribute> &recordDescriptor, const void *data);
    Record(const Record&) = delete;                                     // Copy constructor, implement when needed
    Record(Record&&) = delete;                                          // Move constructor, implement when needed
    Record& operator=(const Record&) = delete;                          // Copy assignment, implement when needed
    Record& operator=(Record&&) = delete;                               // Move assignment, implement when needed
    // free malloc space if data is not passed in.
    ~Record();

    int getSize();
    const void *getRecordData();
    // convert record from the implemented format to the given format
    void convertToRawData(const std::vector<Attribute> &recordDescriptor, void* data);
    void printRecord(const std::vector<Attribute> &recordDescriptor);
};


#endif //CS222_FALL19_RECORD_H