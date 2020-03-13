#include <cstring>
#include <cmath>
#include <vector>
#include <cassert>
#include <iostream>
#include "types.h"

#ifndef CS222_FALL19_RECORD_H
#define CS222_FALL19_RECORD_H


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
    static RecordSize
    getRecordActualSize(const int &nullIndicatorSize, const std::vector<Attribute> &recordDescriptor, const void *data);

    /* Examine a field is null or not, *data is raw record data
     * Note: assume all arguments are valid.
     * */
    static bool isFieldNull(const int &fieldIndex, const void *nullIndicatorData);

public:

    /* Get a specific attribute's index in the descriptor */
    static void findAttrInDescriptor(const std::vector<Attribute> &descriptor, const std::string &attrName, FieldNumber &attrIndex, AttrType &attrType);

    /* Get null indicator data's size by field number */
    static int getNullIndicatorSize(const int &fieldNumber);


    /* convert a descriptor to a string vector */
    static void getDescriptorString(const std::vector<Attribute> &descriptor, std::vector<std::string> &attrNames);

    /* convert raw attribute data to string */
    // here data is points to pure data, eg. for TypeVarchar, data points to char section
    static std::string getAttrString(const void *data, const AttrType &attrType, const AttrLength &attrLength);

    /* compare two data based on type */
    // when type is varchar, data points to "length + string"
    // if data1 > data2, return positive;
    // if data2 < data2, return negative;
    // if data1 == data2, return 0;
    static RC compareRawData(const void *data1, const void *data2, const AttrType &attrType);

    /* create projected descriptor */
    static void createProjectedDescriptor(
            const std::vector<Attribute> &descriptor,
            std::vector<Attribute> &projectedDescriptor,
            const std::vector<std::string> &projectedAttrNames);

    /* get raw attr data length based on attr type */
    static AttrLength getAttrDataLength(const AttrType &attrType, const void* data, bool withNull=false);

private:
    RecordSize size;
    void *record;
    // offset to the start of offset section
    RecordVersion recordVersion{};
    FieldNumber fieldNumber{};
    bool passedData; // indicate whether record space is passed in or created in the class
public:
    explicit Record(void* data);
    Record(const std::vector<Attribute> &recordDescriptor, const void *data, RecordVersion version=0);
    Record(const Record&) = delete;                                     // Copy constructor, implement when needed
    Record(Record&&) = delete;                                          // Move constructor, implement when needed
    Record& operator=(const Record&) = delete;                          // Copy assignment, implement when needed
    Record& operator=(Record&&) = delete;                               // Move assignment, implement when needed
    // free malloc space if data is not passed in.
    ~Record();

    RecordSize getSize();
    const void *getRecordData();
    // convert record from the implemented format to the given format
    void convertToRawData(const std::vector<Attribute> &recordDescriptor, void* data);
    void printRecord(const std::vector<Attribute> &recordDescriptor);

    // if null field, return nullptr
    void *getFieldValue(const FieldNumber &fieldIndex, AttrLength &fieldLength);

    // read attribute into raw data with one byte of null indicator put ahead and varchar with 4 bytes ahead
    // return: 0 - success, -1 - target attribute name not found
    int readAttr(const std::vector<Attribute> &recordDescriptor, const std::string &attributeName, void* data);
};


#endif //CS222_FALL19_RECORD_H
