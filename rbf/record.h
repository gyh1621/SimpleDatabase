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
    TypeInt = 0, TypeReal, TypeVarChar, TypeNull
} AttrType;

typedef unsigned AttrLength;

struct Attribute {
    std::string name;  // attribute name
    AttrType type;     // attribute type
    AttrLength length; // attribute length
};

typedef unsigned short FieldNumber;   // type depends on PAGE_SIZE
typedef unsigned short FieldOffset;   // type depends on PAGE_SIZE
typedef unsigned short RecordVersion;


class Record {

    /*
     * Record Format:
     * ┌─────────────────────────────────┬──────────────────────┬─────────────────────────┐
     * │           RecordHeader          │     OffsetSection    │        FieldData        │
     * ├────────────────┬────────────────┼──────────────────────┼─────────────────────────┤
     * │ Record Version │  Field Number  │  offset, ..., offset │ Field 1 | ... | Field N │
     * │ unsigned short │ unsigned short │    unsigned short    │ ......................  │
     * └────────────────┴────────────────┴──────────────────────┴─────────────────────────┘
     *
     * Note: 1. null field will not occupy a "Field" data space
     *          but will still occupy a "field offset" space and the offset is '0'
     *       2. offset points to end of the data(byte offset points to has no data)
     */

    static const unsigned short RecordHeaderSize = sizeof(FieldNumber) + sizeof(RecordVersion);

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
    // offset to the start of offset section
    RecordVersion recordVersion;
    FieldNumber fieldNumber;
    bool passedData; // indicate whether record space is passed in or created in the class
public:
    Record(void* data);
    Record(const std::vector<Attribute> &recordDescriptor, const void *data, RecordVersion version=0);
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

    void *getFieldValue(const FieldNumber &fieldIndex);
};


#endif //CS222_FALL19_RECORD_H
