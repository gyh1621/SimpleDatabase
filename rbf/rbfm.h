#ifndef _rbfm_h_
#define _rbfm_h_

#include <vector>
#include <cstring>
#include <cassert>
#include "pfm.h"

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

// Comparison Operator (NOT needed for part 1 of the project)
typedef enum {
    EQ_OP = 0, // no condition// =
    LT_OP,      // <
    LE_OP,      // <=
    GT_OP,      // >
    GE_OP,      // >=
    NE_OP,      // !=
    NO_OP       // no condition
} CompOp;


/********************************************************************
* The scan iterator is NOT required to be implemented for Project 1 *
********************************************************************/

# define RBFM_EOF (-1)  // end of a scan operator

//  RBFM_ScanIterator is an iterator to go through records
//  The way to use it is like the following:
//  RBFM_ScanIterator rbfmScanIterator;
//  rbfm.open(..., rbfmScanIterator);
//  while (rbfmScanIterator(rid, data) != RBFM_EOF) {
//    process the data;
//  }
//  rbfmScanIterator.close();

class RBFM_ScanIterator {
public:
    RBFM_ScanIterator() = default;;

    ~RBFM_ScanIterator() = default;;

    // Never keep the results in the memory. When getNextRecord() is called,
    // a satisfying record needs to be fetched from the file.
    // "data" follows the same format as RecordBasedFileManager::insertRecord().
    RC getNextRecord(RID &rid, void *data) { return RBFM_EOF; };

    RC close() { return -1; };
};

class RecordBasedFileManager {
private:

    /* Get first page with larger free size
     * Return:
     *  0: success
     *  -1: fail
     */
    RC getFirstPageAvailable(FileHandle &fileHandle, const int &freeSize, PageNum &pageNum, void *data);

public:
    static RecordBasedFileManager &instance();                          // Access to the _rbf_manager instance

    RC createFile(const std::string &fileName);                         // Create a new record-based file

    RC destroyFile(const std::string &fileName);                        // Destroy a record-based file

    RC openFile(const std::string &fileName, FileHandle &fileHandle);   // Open a record-based file

    RC closeFile(FileHandle &fileHandle);                               // Close a record-based file

    //  Format of the data passed into the function is the following:
    //  [n byte-null-indicators for y fields] [actual value for the first field] [actual value for the second field] ...
    //  1) For y fields, there is n-byte-null-indicators in the beginning of each record.
    //     The value n can be calculated as: ceil(y / 8). (e.g., 5 fields => ceil(5 / 8) = 1. 12 fields => ceil(12 / 8) = 2.)
    //     Each bit represents whether each field value is null or not.
    //     If k-th bit from the left is set to 1, k-th field value is null. We do not include anything in the actual data part.
    //     If k-th bit from the left is set to 0, k-th field contains non-null values.
    //     If there are more than 8 fields, then you need to find the corresponding byte first,
    //     then find a corresponding bit inside that byte.
    //  2) Actual data is a concatenation of values of the attributes.
    //  3) For Int and Real: use 4 bytes to store the value;
    //     For Varchar: use 4 bytes to store the length of characters, then store the actual characters.
    //  !!! The same format is used for updateRecord(), the returned data of readRecord(), and readAttribute().
    // For example, refer to the Q8 of Project 1 wiki page.

    // Insert a record into a file
    RC insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const void *data, RID &rid);

    // Read a record identified by the given rid.
    RC readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const RID &rid, void *data);

    // Print the record that is passed to this utility method.
    // This method will be mainly used for debugging/testing.
    // The format is as follows:
    // field1-name: field1-value  field2-name: field2-value ... \n
    // (e.g., age: 24  height: 6.1  salary: 9000
    //        age: NULL  height: 7.5  salary: 7500)
    RC printRecord(const std::vector<Attribute> &recordDescriptor, const void *data);

    /*****************************************************************************************************
    * IMPORTANT, PLEASE READ: All methods below this comment (other than the constructor and destructor) *
    * are NOT required to be implemented for Project 1                                                   *
    *****************************************************************************************************/
    // Delete a record identified by the given rid.
    RC deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const RID &rid);

    // Assume the RID does not change after an update
    RC updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const void *data,
                    const RID &rid);

    // Read an attribute given its name and the rid.
    RC readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, const RID &rid,
                     const std::string &attributeName, void *data);

    // Scan returns an iterator to allow the caller to go through the results one by one.
    RC scan(FileHandle &fileHandle,
            const std::vector<Attribute> &recordDescriptor,
            const std::string &conditionAttribute,
            const CompOp compOp,                  // comparision type such as "<" and "="
            const void *value,                    // used in the comparison
            const std::vector<std::string> &attributeNames, // a list of projected attributes
            RBFM_ScanIterator &rbfm_ScanIterator);

protected:
    RecordBasedFileManager();                                                   // Prevent construction
    ~RecordBasedFileManager();                                                  // Prevent unwanted destruction
    RecordBasedFileManager(const RecordBasedFileManager &);                     // Prevent construction by copying
    RecordBasedFileManager &operator=(const RecordBasedFileManager &);          // Prevent assignment

};

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
    typedef unsigned short FieldNumber;
    typedef unsigned short FieldOffset;

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
    int fieldNumber;
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

class Page {

    /*
     * Page format:
     * ┌────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
     * │ DATA SECTION: <Record>, <Record>, ....                                                                     │
     * │                                                                                                            │
     * │   ┌──────────────────────────────────┬────────────────┬─────────────────┬────────────────┬─────────────────┤
     * │   │          SLOT DIRECTORY          │ RECORD NUMBER  │   SLOT NUMBER   │   FREE SPACE   │      Inited     │
     * │   ├──────────────────────────────────┼────────────────┼─────────────────┼────────────────┼─────────────────┤
     * │   │     <isPointer, offset, len>     │ record  number │   slot number   │   free bytes   │  init indicator │
     * │   │ <bool, unsigned, unsigned short> │ unsigned short │  unsigned short │ unsigned short │       bool      │
     * └───┴──────────────────────────────────┴────────────────┴─────────────────┴────────────────┴─────────────────┘
     *
     * Note: 1. slots expand from right to left.
     *       2. when slot is a pointer: offset is page id, len is slot id
     *       3. types of offset and len are same to RID's pageNum and slotNum
     *       4. record offset points to the first byte of the record
     */

    typedef unsigned short FreeSpace;
    typedef unsigned short SlotNumber;
    typedef unsigned short RecordNumber;
    typedef bool InitIndicator;
    typedef bool SlotPointerIndicator;
    typedef unsigned RecordOffset;
    typedef unsigned short RecordLength;

public:
    static const unsigned short SlotSize = sizeof(RecordOffset) + sizeof(RecordLength) + sizeof(SlotPointerIndicator);
    static const unsigned short InfoSize = sizeof(RecordNumber) + sizeof(SlotNumber) + sizeof(FreeSpace) + sizeof(InitIndicator);

private:
    bool isInited;
    void *page;
    FreeSpace freeSpace;
    SlotNumber slotNumber;
    RecordNumber recordNumber;
    int freeSpaceOffset;    // start offset of free space

    /* Get nth slot offset from page start
     * n starts from 0 and from right to left
     * */
    int getNthSlotOffset(int n);

    /* Parse a slot
     * slot starts from 0 and from right to left
     * */
    void parseSlot(int slot, SlotPointerIndicator &isPointer, RecordOffset &recordOffset, RecordLength &recordLen);


public:
    // passed page data, will not be delete in destructor
    Page(void *data, bool forceInit=false);
    Page(const Page&) = delete;                                     // copy constructor, implement when needed
    Page(Page&&) = delete;                                          // move constructor, implement when needed
    Page& operator=(const Page&) = delete;                          // copy assignment, implement when needed
    Page& operator=(Page&&) = delete;                               // move assignment, implement when needed
    ~Page();

    /* Insert a record into the page
     * Return: slot id
     */
    int insertRecord(Record &record);

    // if isPointer = true, recordOffset and recordLen returned as newPageId and newRcId
    RC readRecord(const std::vector<Attribute> &recordDescriptor, void* data, SlotPointerIndicator &isPointer,
                  RecordOffset &recordOffset, RecordLength & recordLen);

    const void *getPageData();          // get page data
    const int getFreeSpace();           // get free space
};

#endif // _rbfm_h_
