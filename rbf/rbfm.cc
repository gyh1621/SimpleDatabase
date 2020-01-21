#include "rbfm.h"
#include <cmath>
#include <iostream>

RecordBasedFileManager &RecordBasedFileManager::instance() {
    static RecordBasedFileManager _rbf_manager = RecordBasedFileManager();
    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager() = default;

RecordBasedFileManager::~RecordBasedFileManager() = default;

RecordBasedFileManager::RecordBasedFileManager(const RecordBasedFileManager &) = default;

RecordBasedFileManager &RecordBasedFileManager::operator=(const RecordBasedFileManager &) = default;

RC RecordBasedFileManager::getFirstPageAvailable(FileHandle &fileHandle, const int &freeSize, PageNum &pageNum) {

    int totalPage = fileHandle.getNumberOfPages(), curPage = 0;

    // find a page large enough
    PageFreeSpace pageFreeSpace;
    while (curPage < totalPage) {
        pageFreeSpace = fileHandle.getFreeSpaceOfPage(curPage);
        if (pageFreeSpace >= freeSize) {
            pageNum = curPage;
            return 0;
        }
        curPage++;
    }

    return -1;
}

RC RecordBasedFileManager::createFile(const std::string &fileName) {
    return PagedFileManager::instance().createFile(fileName);
}

RC RecordBasedFileManager::destroyFile(const std::string &fileName) {
    return PagedFileManager::instance().destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
    return PagedFileManager::instance().openFile(fileName, fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    return PagedFileManager::instance().closeFile(fileHandle);
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, RID &rid) {
    Record record(recordDescriptor, data);

    // try to find a page available
    PageNum pageNum;
    int slotID;
    void *pageData = malloc(PAGE_SIZE);
    int neededSize = record.getSize() + Page::SlotSize;
    RC rc = getFirstPageAvailable(fileHandle, neededSize, pageNum);

    if (rc != 0) {
        // fail to find a page available, create a new page
        //std::cout << "create new page to insert needed size " << neededSize << std::endl;  //debug
        Page page(pageData);
        slotID = page.insertRecord(record);
        // write to file
        fileHandle.appendPage(page.getFreeSpace(), page.getPageData());
        pageNum = fileHandle.getNumberOfPages() - 1;
    } else {
        // find a page available, load page data
        fileHandle.readPage(pageNum, pageData);
        Page page(pageData);
        //std::cout << "fina a page to insert needed size " << neededSize << " free space " << page.getFreeSpace() << std::endl; // debug
        slotID = page.insertRecord(record);
        // write to file
        fileHandle.writePage(pageNum, page.getFreeSpace(), page.getPageData());
    }
    //std::cout << "PAGE: " << pageNum << std::endl;  // debug

    rid.pageNum = pageNum;
    rid.slotNum = slotID;

    free(pageData);

    return 0;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                      const RID &rid, void *data) {
    unsigned pageid = rid.pageNum;
    unsigned short slotid = rid.slotNum;
    void* targetPage = malloc(PAGE_SIZE);
    bool isPointer = true;
    RC success = 1;

    while(isPointer){
        fileHandle.readPage(pageid, targetPage);
        Page p(targetPage);
        success = p.readRecord(recordDescriptor, data, isPointer, pageid, slotid);
    }
    //std::cout << "PAGE " << pageid << " SLOT " << slotid << std::endl;  // debug
    free(targetPage);
    return success;
}

RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const RID &rid) {
    return -1;
}

RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data) {
    Record r(recordDescriptor, data);
    r.printRecord(recordDescriptor);
    return 0;
}

RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                        const void *data, const RID &rid) {
    return -1;
}

RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                         const RID &rid, const std::string &attributeName, void *data) {
    return -1;
}

RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                const std::vector<std::string> &attributeNames, RBFM_ScanIterator &rbfm_ScanIterator) {
    return -1;
}

// ========================================================================================
//                                     Record Class
// ========================================================================================

int Record::getNullIndicatorSize(const int &fieldNumber) {
    return ceil(fieldNumber / 8.0);
}

bool Record::isFieldNull(const int &fieldIndex, const void *nullIndicatorData) {
    // get the byte field-bit inside
    int byteIndex = fieldIndex / 8;
    unsigned nullIndicator;
    memcpy(&nullIndicator, (char *) nullIndicatorData + byteIndex, sizeof(unsigned));
    // get field bit's index in the byte
    unsigned fieldBitIndex = 8 - fieldIndex % 8 - 1;
    // get field bit
    unsigned fieldBit = (nullIndicator & ( 1 << fieldBitIndex)) >> fieldBitIndex;
    return fieldBit == 1;
}

int Record::getRecordActualSize(const int &nullIndicatorSize, const std::vector<Attribute> &recordDescriptor, const void *data) {
    // get initial size: header size + field offset section size
    int size = Record::recordHeaderSize + recordDescriptor.size() * sizeof(FieldOffset);
    int offset = nullIndicatorSize;

    // add fields' data size
    auto *charLength = new unsigned;
    for (auto it = recordDescriptor.begin(); it != recordDescriptor.end(); it++) {
        int fieldIndex = std::distance(recordDescriptor.begin(), it);
        if (isFieldNull(fieldIndex, data)) continue;
        switch (it->type) {
            case AttrType::TypeInt: size += sizeof(int); offset += sizeof(int); break;
            case AttrType::TypeReal: size += sizeof(float); offset += sizeof(int); break;
            case AttrType::TypeVarChar:
                memcpy(charLength, (char *) data + offset, sizeof(unsigned));
                //unsigned charLength = *((unsigned *) ((char *) data + offset));  // 4 bytes in raw data
                size += *charLength;
                offset += sizeof(unsigned) + *charLength;
                break;
        }
    }
    delete(charLength);

    return size;
}

Record::Record(const std::vector<Attribute> &recordDescriptor, const void *data) {
    passedData = false;
    int fieldNum = recordDescriptor.size();
    int nullIndicatorSize = getNullIndicatorSize(fieldNum);
    this->size = getRecordActualSize(nullIndicatorSize, recordDescriptor, data);
    this->record = malloc(this->size);
    if (this->record == nullptr) throw std::bad_alloc();
    this->offsetSectionOffset = recordHeaderSize;
    this->fieldNumber = fieldNum;
    // offset1 is the current writing position of *record
    // offset2 is the current reading position of *data
    int offset1 = 0, offset2 = 0;

    // write field number
    *((FieldNumber *) record) = fieldNum;
    offset1 += sizeof(FieldNumber);

    // record: jump over offset section
    offset1 += recordDescriptor.size() * sizeof(FieldOffset);

    // data: jump over null indicator data
    offset2 += nullIndicatorSize;

    // copying fields
    bool nullBit;
    auto *varcharLength = new unsigned;
    for (auto it = recordDescriptor.begin(); it != recordDescriptor.end(); it++) {
        int index = std::distance(recordDescriptor.begin(), it);  // field index
        nullBit = isFieldNull(index, data);  // get null bit
        int currentFieldOffsetPos = offsetSectionOffset + index * sizeof(FieldOffset);
        if (nullBit) {
            // null field
            *((FieldOffset *)((char *) record + currentFieldOffsetPos)) = 0;
        } else {
            // copy field
            AttrType type = it->type;
            if (type == AttrType::TypeInt) {
                memcpy((char *) record + offset1, (char *) data + offset2, it->length);
                offset1 += it->length;
                offset2 += it->length;
            } else if (type == AttrType::TypeReal) {
                memcpy((char *) record + offset1, (char *) data + offset2, it->length);
                offset1 += it->length;
                offset2 += it->length;
            } else if (type == AttrType::TypeVarChar) {
                // read length byte
                memcpy(varcharLength, (char *) data + offset2, sizeof(unsigned));
                offset2 += sizeof(unsigned);
                // copy varchar
                memcpy((char *) record + offset1, (char *) data + offset2, *varcharLength);
                offset1 += *varcharLength;
                offset2 += *varcharLength;
            }
            *((FieldOffset *)((char *) record + currentFieldOffsetPos)) = (FieldOffset) offset1;
        }
    }
    delete(varcharLength);

    assert(this->size == offset1);
}

Record::Record(void* data){
    passedData = true;
    // TODO: wrong
    this->size = sizeof(data);
    this->record = data;
    FieldNumber headerNum = *(FieldNumber *) data;
    this->fieldNumber = headerNum;
    this->offsetSectionOffset = sizeof(FieldNumber) * (headerNum + 1);
}

int Record::getSize() { return size; }

const void * Record::getRecordData() {
    return record;
}

void Record::convertToRawData(const std::vector<Attribute> &recordDescriptor, void *data) {
    // init null indicators
    int nullPointerSize = getNullIndicatorSize(fieldNumber);
    auto* nullPointer = (unsigned char*)data;
    for (int i = 0; i < nullPointerSize; i++) nullPointer[i] = 0;

    // jump over record header
    int startOffset = recordHeaderSize;

    // field offsets
    int lastFieldEndOffset = startOffset + fieldNumber * sizeof(FieldOffset);
    int fieldEndOffset;
    // pointer of field value section of data
    int dataOffset = nullPointerSize;

    for(int i = 0; i < fieldNumber; i++){
        fieldEndOffset = *((FieldOffset *)((char *)record + startOffset + sizeof(FieldOffset) * i));
        int byteNum = i / 8;
        Attribute attr = recordDescriptor[i];
        int fieldLength = fieldEndOffset - lastFieldEndOffset;
        if(fieldEndOffset == 0){  // null field
            nullPointer[byteNum] |= ((char) 1) << (8 - (i % 8) - 1);
        } else {  // data field
            if (attr.type == AttrType::TypeVarChar) {
                memcpy((char*)data + dataOffset, &fieldLength, sizeof(unsigned));
                dataOffset += 4;
            }
            memcpy((char*)data + dataOffset, (char *)record + lastFieldEndOffset, fieldLength);
            dataOffset += fieldLength;
            lastFieldEndOffset = fieldEndOffset;
        }
    }
}

void Record::printRecord(const std::vector<Attribute> &recordDescriptor) {
    // jump over record header
    int startOffset = recordHeaderSize;

    // field offsets
    int lastFieldEndOffset = startOffset + fieldNumber * sizeof(FieldOffset);
    int fieldEndOffset;

    int intAttrVal;
    float realAttrVal;
    for (int i = 0; i < fieldNumber; i++) {
        Attribute attr = recordDescriptor[i];
        std::cout << attr.name << ": ";
        fieldEndOffset = *((FieldOffset *)((char *)record + startOffset + sizeof(FieldOffset) * i));
        int fieldLength = fieldEndOffset - lastFieldEndOffset;
        if (fieldEndOffset == 0){
            std::cout << "NULL";
        } else {
            switch(attr.type){
                case AttrType::TypeInt:
                    memcpy(&intAttrVal, (char *)record + lastFieldEndOffset, sizeof(int));
                    std::cout << intAttrVal;
                    break;
                case AttrType::TypeReal:
                    memcpy(&realAttrVal, (char *)record + lastFieldEndOffset, sizeof(float));
                    std::cout << realAttrVal;
                    break;
                case AttrType::TypeVarChar:
                    char* varchar = new char[fieldLength];
                    memcpy(varchar, (char *) record + lastFieldEndOffset, fieldLength);
                    std::string s(varchar, fieldLength);
                    std::cout << s;
                    delete[](varchar);
                    break;
            }
            lastFieldEndOffset = fieldEndOffset;
        }
        if (i == fieldNumber - 1) {
            std::cout << std::endl;
        } else {
            std::cout << " ";
        }
    }
}

Record::~Record() {
    if (!passedData) free(this->record);
};

// ========================================================================================
//                                     Page Class
// ========================================================================================

Page::Page(void *data) {
    page = data;

    // Init page
    int recordNumberOffset = PAGE_SIZE - InfoSize;
    int slotNumberOffset = recordNumberOffset + sizeof(RecordNumber);
    int initIndicatorOffset = PAGE_SIZE - sizeof(InitIndicator);
    InitIndicator isInited;
    memcpy(&isInited, (char *) page + initIndicatorOffset, sizeof(InitIndicator));
    if (isInited == 'Y') {
        // read last info section
        memcpy(&recordNumber, (char *) page + recordNumberOffset, sizeof(RecordNumber));
        memcpy(&slotNumber, (char *) page + slotNumberOffset, sizeof(SlotNumber));
    } else {
        // init page info
        recordNumber = 0;
        slotNumber = 0;
        isInited = 'Y';
        memcpy((char *) page + initIndicatorOffset, &isInited, sizeof(InitIndicator));
    }
    // compute free space start offset and free space
    int slot = slotNumber - 1;
    freeSpaceOffset = 0;
    SlotPointerIndicator isPointer;
    RecordOffset recordOffset;
    RecordLength recordLength;
    while (slot >= 0) {
        parseSlot(slot, isPointer, recordOffset, recordLength);
        if (!isPointer) {
            freeSpaceOffset = recordOffset + recordLength;
            break;
        }
        slot--;
    }
    freeSpace = getNthSlotOffset(slotNumber - 1) - freeSpaceOffset;
}

Page::~Page() { }

int Page::getNthSlotOffset(int n) {
//    assert(n < slotNumber);
    return PAGE_SIZE - (InfoSize + SlotSize * (n + 1));
}

void Page::parseSlot(int slot, Page::SlotPointerIndicator &isPointer, Page::RecordOffset &recordOffset,
                     Page::RecordLength &recordLen){
    int slotOffset = getNthSlotOffset(slot);
    isPointer = *(SlotPointerIndicator *)((char *) page + slotOffset);
    slotOffset += sizeof(SlotPointerIndicator);
    memcpy(&recordOffset, (char *) page + slotOffset, sizeof(RecordOffset));
    slotOffset += sizeof(RecordOffset);
    memcpy(&recordLen, (char *) page + slotOffset, sizeof(RecordLength));
}

int Page::insertRecord(Record &record) {
    // insert record
    int startOffset = freeSpaceOffset;
    memcpy((char *) page + freeSpaceOffset, record.getRecordData(), record.getSize());
    //std::cout << "Insert one record " << "at: " << startOffset  // debug
    //          << " end:" << startOffset + record.getSize() << "\t";
    recordNumber++;
    // update free space
    freeSpaceOffset += record.getSize();
    freeSpace -= record.getSize();
    freeSpace -= SlotSize;
    // insert the corresponding slot
    int offset = getNthSlotOffset(slotNumber);
    //std::cout << " SLOT at: " << offset << " end: " << offset + SlotSize << "\t";  // debug
    *((SlotPointerIndicator *) ((char *) page + offset)) = false;
    offset += sizeof(SlotPointerIndicator);
    memcpy((char *) page + offset, &startOffset, sizeof(RecordOffset));
    offset += sizeof(RecordOffset);
    int recordSize = record.getSize();
    memcpy((char *) page + offset, &recordSize, sizeof(RecordLength));
    slotNumber++;
    return slotNumber - 1;  // slot index starts from 0
}

RC Page::readRecord(const std::vector<Attribute> &recordDescriptor, void* data,
                    bool &isPointer, unsigned &pageNum, unsigned short &slotNum){
    RecordOffset offset;
    RecordLength length;

    parseSlot(slotNum,isPointer, offset, length);
    if(isPointer){
        pageNum = offset;
        slotNum = length;
        return -1;
    }

    void* record = malloc(length);
    memcpy((char *) record, (char *)page + offset, length);
    Record r(record);
    r.convertToRawData(recordDescriptor, data);
    free(record);
    //std::cout << "Read one record at " << offset << " end " << offset + length << "\t"; // debug
    return 0;
}

const void * Page::getPageData() {
    int offset = PAGE_SIZE - InfoSize;
    memcpy((char *) page + offset, &recordNumber, sizeof(RecordNumber));
    offset += sizeof(RecordNumber);
    memcpy((char *) page + offset, &slotNumber, sizeof(SlotNumber));
    return page;
}

const int Page::getFreeSpace() { return freeSpace; }