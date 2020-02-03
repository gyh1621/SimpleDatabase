#include "record.h"

#ifndef CS222_FALL19_PAGE_H
#define CS222_FALL19_PAGE_H

#define PAGE_SIZE 4096
#define PAGES_IN_FSP (PAGE_SIZE / sizeof(PageFreeSpace))

typedef unsigned PageNum;
typedef unsigned short PageFreeSpace;   // depends on PAGE_SIZE

typedef unsigned short SlotNumber;      // number of slots in a page, depends on PAGE_SIZE
typedef unsigned short RecordNumber;    // depends on PAGE_SIZE
typedef char InitIndicator;
typedef bool SlotPointerIndicator;
typedef unsigned RecordOffset;          // depends on PageNum
typedef unsigned short RecordLength;    // depends on PAGE_SIZE


class FreeSpacePage {

    /* Free Space Page Format:
     *
     * https://docs.microsoft.com/en-us/sql/relational-databases/pages-and-extents-architecture-guide?view=sql-server-ver15#tracking-free-space
     *
     * Different from implementation above, the FreeSpacePage will store accurate FreeSpace in the page, so when
     * PAGE_SIZE is 4096 and FreeSpace is unsigned short, each FSP can hold 2048 pages' free space.
     *
     * For FSP organization in database files, see comment of FileHandle::changeToActualPageNum
     *
     */

private:
    void *page;

public:
    FreeSpacePage() = default;
    // passed page data, will not be delete in destructor
    FreeSpacePage(void *data);
    FreeSpacePage(const FreeSpacePage&) = delete;                          // copy constructor, implement when needed
    FreeSpacePage(FreeSpacePage&&) = delete;                               // move constructor, implement when needed
    FreeSpacePage& operator=(const FreeSpacePage&) = delete;               // copy assignment, implement when needed
    FreeSpacePage& operator=(FreeSpacePage&&) = delete;                    // move assignment, implement when needed
    ~FreeSpacePage() = default;

    void loadNewPage(void *data);       // load another fsp

    // pageIndex indicates the page's index in this free space page, starts from 0
    void writeFreeSpace(PageNum pageIndex, PageFreeSpace freePageSpace);
    PageFreeSpace getFreeSpace(PageNum pageIndex);
};


class DataPage {

    /*
     * Page format:
     * ┌───────────────────────────────────────────────────────────────────────────────────────────┐
     * │ DATA SECTION: <Record>, <Record>, ....                                                    │
     * │                                                                                           │
     * │   ┌──────────────────────────────────┬────────────────┬─────────────────┬─────────────────┤
     * │   │          SLOT DIRECTORY          │ RECORD NUMBER  │   SLOT NUMBER   │      Inited     │
     * │   ├──────────────────────────────────┼────────────────┼─────────────────┼─────────────────┤
     * │   │     <isPointer, offset, len>     │ record  number │   slot number   │  init indicator │
     * │   │ <bool, unsigned, unsigned short> │ unsigned short │  unsigned short │       char      │
     * └───┴──────────────────────────────────┴────────────────┴─────────────────┴─────────────────┘
     *
     * Note: 1. slots expand from right to left.
     *       2. when slot is a pointer: offset is page id, len is slot id
     *       3. types of offset and len are same to RID's pageNum and slotNum
     *       4. record offset points to the first byte of the record
     *       5. once a slot is created, it will never be deleted. If corresponding record is deleted,
     *          the slot will marked as Deleted but still exists. If corresponding record is moved to
     *          another page, the slot will become a pointer.
     */


public:
    static const unsigned SlotSize = sizeof(RecordOffset) + sizeof(RecordLength) + sizeof(SlotPointerIndicator);
    static const unsigned InfoSize = sizeof(RecordNumber) + sizeof(SlotNumber) + sizeof(InitIndicator);
    static const RecordOffset DeletedRecordOffset = PAGE_SIZE;

private:
    void *page;
    PageFreeSpace freeSpace;
    SlotNumber slotNumber;
    RecordNumber recordNumber;
    RecordOffset freeSpaceOffset;    // start offset of free space

    /* Get nth slot offset from page start
     * n starts from 0 and from right to left
     * */
    static int getNthSlotOffset(SlotNumber n);

    /* Parse a slot
     * slot starts from 0 and from right to left
     * */
    void parseSlot(int slot, SlotPointerIndicator &isPointer, RecordOffset &recordOffset, RecordLength &recordLen);

    /* Get first available slot */
    SlotNumber getFirstAvailableSlot();

    //after move, update slot info that has been moved. dir = true : forward, false: backward;
    void updateSlotInfo(RecordOffset offset, RecordLength length, bool dir);

    // move records included from startOffset to final record's end, not doing any check here
    void moveRecords(RecordOffset startOffset, RecordOffset targetOffset);

public:
    // passed page data, will not be delete in destructor
    explicit DataPage(void *data);
    DataPage(const DataPage&) = delete;                             // copy constructor, implement when needed
    DataPage(DataPage&&) = delete;                                  // move constructor, implement when needed
    DataPage& operator=(const DataPage&) = delete;                  // copy assignment, implement when needed
    DataPage& operator=(DataPage&&) = delete;                       // move assignment, implement when needed
    ~DataPage() = default;

    // set target slot to indicate that record is deleted;
    // offset will be set to PAGE_SIZE
    void deleteSlot(int slot);

    /* Insert a record into the page
     * Return: slot id
     */
    SlotNumber insertRecord(Record &record);

    /* get record length, always assume record exists in this page */
    RecordLength getRecordSize(SlotNumber slot);

    /* get record formatted raw data */
    // if slot is not an actual record, return nullptr
    void *readRecord(SlotNumber slot);

    // always assume record exists in this page
    void readRecordIntoRaw(SlotNumber slot, const std::vector<Attribute> &recordDescriptor, void* data);

    // delete target record, always assume record exists in this page
    void deleteRecord(SlotNumber &slotid);

    // a record will be moved another page
    // delete record and change the slot into a pointer
    void moveRecord(SlotNumber slot, const RID &newRID);

    // update target record;
    void updateRecord(Record &updateRecord, RecordLength &slotid);

    /* Return: 0 - exists, -1 - pointer, 1 - deleted record */
    int checkRecordExist(SlotNumber &slotid, RID &newRID);
    const void *getPageData();          // get page data
    SlotNumber getSlotNumber();         // get page's slot number
    const PageFreeSpace getFreeSpace();           // get free space
    const RecordNumber getRecordNumber();
};


#endif //CS222_FALL19_PAGE_H
