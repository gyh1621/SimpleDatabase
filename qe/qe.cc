#include <sstream>
#include <iostream>
#include "qe.h"

bool Iterator::isAttrDataNull(const void *data) {
    bool nullBit = *(unsigned char *) ((char *) data) & ((unsigned) 1 << (unsigned) 7);
    return nullBit;
}

void Iterator::concatenateDescriptor(const std::vector<Attribute> &descriptor1,
        const std::vector<Attribute> &descriptor2, std::vector<Attribute> &resultDescriptor) {
    resultDescriptor.clear();
    for (const auto & attr : descriptor1) {
        resultDescriptor.push_back(attr);
    }
    for (const auto & attr : descriptor2) {
        resultDescriptor.push_back(attr);
    }
}

void Iterator::concatenateRecords(const void *leftData, const void *rightData, const std::vector<Attribute> &leftDescriptor,
                                  const std::vector<Attribute> &rightDescriptor, void *data) {
    auto newSize = leftDescriptor.size() + rightDescriptor.size();
    // init null indicators
    auto nullPointerSize = Record::getNullIndicatorSize(newSize);
    auto* nullPointer = (unsigned char*)data;
    for (int i = 0; i < nullPointerSize; i++) nullPointer[i] = 0;

    Record leftRecord(leftDescriptor, leftData);
    Record rightRecord(rightDescriptor, rightData);
    //void* fieldValue = malloc(TUPLE_TMP_SIZE);
    int dataOffset = nullPointerSize;
    for (int i = 0; i < leftDescriptor.size(); i++) {
        AttrLength attrLength;
        void* fieldValue = leftRecord.getFieldValue(i, attrLength);
        if(fieldValue == nullptr) {
            int byteNum = i / 8;
            nullPointer[byteNum] |= ((char) 1) << (8 - (i % 8) - 1);
            free(fieldValue);
            continue;
        }
        switch (leftDescriptor[i].type) {
            case TypeInt:
            case TypeReal:
                memcpy((char *) data + dataOffset, fieldValue, attrLength);
                dataOffset += attrLength;
                break;
            case TypeVarChar:
                memcpy((char *) data + dataOffset, &attrLength, 4);
                dataOffset += 4;
                memcpy((char *) data + dataOffset, fieldValue, attrLength);
                dataOffset += attrLength;
                break;
            default: throw std::invalid_argument("unknown attr type.");
        }
        free(fieldValue);
    }

    for (int i = 0; i < rightDescriptor.size(); i++) {
        AttrLength attrLength;
        void *fieldValue = rightRecord.getFieldValue(i, attrLength);
        if(fieldValue == nullptr) {
            int byteNum = (i + leftDescriptor.size()) / 8;
            nullPointer[byteNum] |= ((char) 1) << (8 - ((i + leftDescriptor.size()) % 8) - 1);
            free(fieldValue);
            continue;
        }
        switch (rightDescriptor[i].type) {
            case TypeInt:
            case TypeReal:
                memcpy((char *) data + dataOffset, fieldValue, attrLength);
                dataOffset += attrLength;
                break;
            case TypeVarChar:
                memcpy((char *) data + dataOffset, &attrLength, 4);
                dataOffset += 4;
                memcpy((char *) data + dataOffset, fieldValue, attrLength);
                dataOffset += attrLength;
                break;
            default: throw std::invalid_argument("unknown attr type.");
        }
        free(fieldValue);
    }
}

Filter::Filter(Iterator *input, const Condition &condition) {
    this->input = input;
    this->condition = condition;
    // get record descriptor
    this->input->getAttributes(this->recordDescriptor);
}

RC Filter::getNextTuple(void *data) {
    RC rc = this->input->getNextTuple(data);
    if (rc == QE_EOF) {
        return rc;
    }
    if (condition.op == NO_OP) {
        return 0;
    }

    while (rc != QE_EOF) {
        Record r(recordDescriptor, data);

        // read condition's left value
        void *leftData = malloc(PAGE_SIZE);
        if (leftData == nullptr) throw std::bad_alloc();
        memset(leftData, 0, PAGE_SIZE);
        r.readAttr(recordDescriptor, condition.lhsAttr, leftData);

        // read condition's right value
        void *rightData = malloc(PAGE_SIZE);
        if (rightData == nullptr) throw std::bad_alloc();
        memset(rightData, 0, PAGE_SIZE);
        if (condition.bRhsIsAttr) {
            // should not happen
            throw std::invalid_argument("filter condition's right is an attribute");
            //r.readAttr(recordDescriptor, condition.rhsAttr, rightData);
        } else {
            *((bool *) rightData) = false;
            switch (condition.rhsValue.type) {
                case TypeInt:
                    memcpy((char *) rightData + 1, condition.rhsValue.data, sizeof(int));
                    break;
                case TypeReal:
                    memcpy((char *) rightData + 1, condition.rhsValue.data, sizeof(float));
                    break;
                case TypeVarChar:
                    AttrLength length;
                    memcpy(&length, condition.rhsValue.data, 4);
                    memcpy((char *) rightData + 1, (char *) condition.rhsValue.data, length + 4);
                    break;
                default:
                    throw std::invalid_argument("unknown attribute type");
            }
        }

        // both null
        if (Iterator::isAttrDataNull(leftData) && Iterator::isAttrDataNull(rightData)) {
            throw std::exception();
        }

        // compare
        bool success = false;

        // left data is NULL
        if (Iterator::isAttrDataNull(leftData)) {
            switch (condition.op) {
                case LT_OP:
                case LE_OP:
                case NE_OP:
                    success = true;
                    break;
                default:
                    success = false;
                    break;
            }
        }
        // right data is NULL
        else if (Iterator::isAttrDataNull(rightData)) {
            switch (condition.op) {
                case GT_OP:
                case GE_OP:
                case NE_OP:
                    success = true;
                    break;
                default: break;
            }
        }
        // both not null
        else {
            int res = Record::compareRawData((char *) leftData + 1, (char *) rightData + 1, condition.rhsValue.type);
            // left data > right data
            if (res > 0) {
                switch (condition.op) {
                    case GT_OP:
                    case GE_OP:
                        success = true;
                        break;
                    default: break;
                }
            }
            // left data < right data
            else if (res < 0) {
                switch (condition.op) {
                    case LT_OP:
                    case LE_OP:
                        success = true;
                        break;
                    default: break;
                }
            }
            // left data == right data
            else {
                switch (condition.op) {
                    case LE_OP:
                    case GE_OP:
                    case EQ_OP:
                        success = true;
                        break;
                    default: break;
                }
            }
        }

        // release memory
        free(leftData);
        free(rightData);

        if (success) {
            return 0;
        }

        // get next data
        rc = input->getNextTuple(data);
    }

    return QE_EOF;
}

void Filter::getAttributes(std::vector<Attribute> &attrs) const {
    this->input->getAttributes(attrs);
}


Project::Project(Iterator *input, const std::vector<std::string> &attrNames) {
    this->input = input;
    input->getAttributes(attributes);
    Record::createProjectedDescriptor(attributes, projDescriptor, attrNames);
    for (const std::string& attrName: attrNames) {
        for (const Attribute& attr: attributes) {
            if (attr.name == attrName) {
                projAttributes.push_back(attr);
                break;
            }
        }
    }
    assert(attrNames.size() == projAttributes.size());
}

RC Project::getNextTuple(void *data) {
    RC rc = input->getNextTuple(data);
    if (rc == QE_EOF) {
        return rc;
    }

    Record r(attributes, data);
    r.convertToRawData(projDescriptor, data);
    return 0;
}

void Project::getAttributes(std::vector<Attribute> &attrs) const {
    attrs = projAttributes;
}

INLJoin::INLJoin(Iterator *leftIn, IndexScan *rightIn, const Condition &condition) {
    this->leftIn = leftIn;
    this->rightIn = rightIn;
    this->condition = condition;

    this->leftIn->getAttributes(this->leftDescriptor);
    this->rightIn->getAttributes(this->rightDescriptor);
    Iterator::concatenateDescriptor(leftDescriptor, rightDescriptor, this->concatenateDescriptor);

    this->rightTupleFinish = QE_EOF;
    this->leftData = malloc(TUPLE_TMP_SIZE);
    this->leftTupleFinish = this->leftIn->getNextTuple(this->leftData);
    void* key;
    key = setKey(leftDescriptor, condition.lhsAttr, leftData);
    rightIn->setIterator(key, key, true, true);
    free(key);
}

RC INLJoin::getNextTuple(void *data) {
    if (leftTupleFinish == QE_EOF) return QE_EOF;
    void* rightData = malloc(TUPLE_TMP_SIZE);
    rightTupleFinish = rightIn->getNextTuple(rightData);
    while (rightTupleFinish == QE_EOF) {
        leftTupleFinish = leftIn->getNextTuple(leftData);
        if (leftTupleFinish == QE_EOF) {
            free(rightData);
            return QE_EOF;
        }
        void* key;
        key = INLJoin::setKey(leftDescriptor, condition.lhsAttr, leftData);
        rightIn->setIterator(key, key, true, true);
        free(key);
        rightTupleFinish = rightIn->getNextTuple(rightData);
    }

    Iterator::concatenateRecords(leftData, rightData, leftDescriptor, rightDescriptor, data);
    free(rightData);
    return 0;
}

void* INLJoin::setKey(const std::vector<Attribute>& descriptor, const std::string &attrName, const void *leftData) {
    void* key = malloc(TUPLE_TMP_SIZE);
    Record record(descriptor, leftData);
    record.readAttr(descriptor, attrName, key);
    int i;
    for (i = 0; i < descriptor.size(); i++) {
        if (descriptor[i].name == attrName) {
            break;
        }
    }
    AttrLength attrLength = Record::getAttrDataLength(descriptor[i].type, key, true);
    void* returnedKey = malloc(attrLength);
    memcpy((char *) returnedKey, (char *) key + 1, attrLength);
    free(key);
    return returnedKey;
}

void INLJoin::getAttributes(std::vector<Attribute> &attrs) const {
    attrs =  this->concatenateDescriptor;
}

INLJoin::~INLJoin() {
    free(leftData);
}


BNLJoin::BNLJoin(Iterator *leftIn, TableScan *rightIn, const Condition &condition, const PageNum numPages) {
    this->leftIn = leftIn;
    this->rightIn = rightIn;
    this->condition = condition;
    this->numPages = numPages;
    leftIn->getAttributes(this->leftDescriptor);
    rightIn->getAttributes(this->rightDescriptor);
    Iterator::concatenateDescriptor(leftDescriptor, rightDescriptor, this->concatenateDescriptor);
    this->conditionAttrType = TypeNull;
    for (const Attribute &attr: this->leftDescriptor) {
        if (attr.name == condition.lhsAttr) {
            this->conditionAttrType = attr.type;
            break;
        }
    }
    assert(this->conditionAttrType != TypeNull);
    this->rightConditionAttrIndex = this->rightDescriptor.size();
    for (FieldNumber i = 0; i < this->rightDescriptor.size(); i++) {
        if (rightDescriptor[i].name == condition.rhsAttr) {
            this->rightConditionAttrIndex = i;
            break;
        }
    }
    assert(this->rightConditionAttrIndex != this->rightDescriptor.size());
    rightData = malloc(TUPLE_TMP_SIZE);
    curLRIndex = -1;
    leftInputFinish = rightInputFinish = QE_EOF + 1;
}

RC BNLJoin::readBlock(Iterator *input, std::map<std::string, std::vector<void *>> &map,
                        const std::string &attrName, const AttrType &attrType, const PageNum &numPages) {
    PageOffset currentSize = 0;
    void *rData = malloc(TUPLE_TMP_SIZE);
    void *aData = malloc(TUPLE_TMP_SIZE);
    std::vector<Attribute> descriptor;
    input->getAttributes(descriptor);

    RC finish = QE_EOF;
    while (currentSize <= numPages * PAGE_SIZE) {
        RC rc = input->getNextTuple(rData);
        if (rc == QE_EOF) {
            break;
        }

        finish = QE_EOF + 1;  // not reach end

        Record r(descriptor, rData);
        RC res = r.readAttr(descriptor, attrName, aData);
        assert(res == 0);

        AttrLength attrLength = Record::getAttrDataLength(attrType, aData, true);
        std::string attrVal = Record::getAttrString((char *) aData + 1, attrType, attrLength);

        void *record = malloc(r.getSize());
        memcpy(record, rData, r.getSize());
        if (map.count(attrVal) == 0) {  // key not exist
            std::vector<void *> records;
            records.push_back(record);
            map.insert(std::make_pair(attrVal, records));
        } else {  // key exists
            map[attrVal].push_back(record);
        }

        currentSize += r.getSize();
    }

    free(rData);
    free(aData);
    return finish;
}

 unsigned GHJoin::joinID = 0;

GHJoin::GHJoin(Iterator *leftIn, Iterator *rightIn, const Condition &condition, const unsigned numPartitions) {
    this->curJoinID = GHJoin::joinID++;
    leftIn->getAttributes(this->leftDescriptor);
    rightIn->getAttributes(this->rightDescriptor);
    Record::getDescriptorString(leftDescriptor, leftDesNames);
    Record::getDescriptorString(rightDescriptor, rightDesNames);
    Iterator::concatenateDescriptor(leftDescriptor, rightDescriptor, this->concatenateDescriptor);
    this->numPartitions = numPartitions;
    this->condition = condition;
    GHJoin::createPartitions("left", leftIn, condition.lhsAttr, numPartitions, curJoinID);
    GHJoin::createPartitions("right", rightIn, condition.rhsAttr, numPartitions, curJoinID);
    curParID = 0;
    Record::findAttrInDescriptor(leftDescriptor, condition.lhsAttr, leftCondAttrIndex, condAttrType);
    Record::findAttrInDescriptor(rightDescriptor, condition.rhsAttr, rightCondAttrIndex, condAttrType);
    loadNextPartitions();
}

std::string GHJoin::getPartitionName(const std::string &partName, const unsigned &pID, const unsigned &joinID) {
    return partName + "_p" + std::to_string(pID) + "_j" + std::to_string(joinID);
}

void GHJoin::createPartitions(const std::string &partName, Iterator *input, const std::string &attrName,
                              const unsigned &numPartitions, const unsigned &jID) {
    std::vector<FileHandle *> fileHandles;
    for (auto i = 0; i < numPartitions; i++) {
        fileHandles.push_back(new FileHandle());
    }
    // create partition files and get file handles
    for (auto i = 0; i < numPartitions; i++) {
        std::string parFileName = GHJoin::getPartitionName(partName, i, jID);
        RC rc = RecordBasedFileManager::instance().createFile(parFileName);
        assert(rc == 0 && "create partition file fails.");
        FileHandle *fileHandle = fileHandles[i];
        RecordBasedFileManager::instance().openFile(parFileName, *fileHandle);
    }
    // get record descriptor and attr index
    std::vector<Attribute> descriptor;
    input->getAttributes(descriptor);
    FieldNumber attrIndex = descriptor.size();
    AttrLength fieldLength;
    AttrType attrType = TypeNull;
    Record::findAttrInDescriptor(descriptor, attrName, attrIndex, attrType);
    assert(attrIndex != descriptor.size());
    // put records into partitions
    void *data = malloc(TUPLE_TMP_SIZE);
    memset(data, 0, TUPLE_TMP_SIZE);
    RID rid;

    while (input->getNextTuple(data) != QE_EOF) {
        Record r(descriptor, data);
        void *attrData = r.getFieldValue(attrIndex, fieldLength);
        std::string attrStr = Record::getAttrString(attrData, attrType, fieldLength);
        if (attrData != nullptr) free(attrData);
        unsigned pID = GHJoin::hash(attrStr, numPartitions);
        RC rc = RecordBasedFileManager::instance().insertRecord(*fileHandles[pID], descriptor, data, rid);
        assert(rc == 0);
    }
    free(data);

    // close file handles
    for (auto fileHandle: fileHandles) {
        RC rc = RecordBasedFileManager::instance().closeFile(*fileHandle);
        assert(rc == 0);
        delete(fileHandle);
    }
}

unsigned GHJoin::hash(const std::string &str, const unsigned &maxValue) {
    unsigned sum = 0;
    for (char i : str) {
        sum += i;
    }
    return sum % maxValue;
}

void GHJoin::clearHashTable(std::map<std::string, std::vector<void *>> &map) {
    for (auto & it : map) {
        for (auto pointer: it.second) {
            free(pointer);
        }
    }
    map.clear();
}

void GHJoin::createHashTable(FileHandle &fileHandle, const std::vector<Attribute> &descriptor,
                             const FieldNumber &attrIndex, std::map<std::string, std::vector<void *>> &map) {
    std::vector<std::string> desNames;
    Record::getDescriptorString(descriptor, desNames);
    RBFM_ScanIterator scanner;
    RecordBasedFileManager::instance().scan(fileHandle, descriptor, "", NO_OP, nullptr, desNames, scanner);
    RID rid;
    void *data = malloc(TUPLE_TMP_SIZE);
    AttrLength attrLength;
    while (scanner.getNextRecord(rid, data) != RBFM_EOF) {
        Record r(descriptor, data);
        void *attrData = r.getFieldValue(attrIndex, attrLength);
        std::string attrStr = Record::getAttrString(attrData, descriptor[attrIndex].type, attrLength);
        void *record = malloc(r.getSize());
        memcpy(record, data, r.getSize());
        if (map.count(attrStr) == 0) {  // key not exist
            std::vector<void *> records;
            records.push_back(record);
            map.insert(std::make_pair(attrStr, records));
        } else {  // key exists
            map[attrStr].push_back(record);
        }
        free(attrData);
    }
    free(data);
}

void GHJoin::loadNextPartitions() {
    if (curParID >= numPartitions) throw std::invalid_argument("curParID exceeds range.");
    std::string leftParName = GHJoin::getPartitionName("left", curParID, curJoinID);
    std::string rightParName = GHJoin::getPartitionName("right", curParID, curJoinID);
    // compare two partition size, find smaller one
    FileHandle leftHandle, rightHandle;
    RecordBasedFileManager::instance().openFile(leftParName, leftHandle);
    RecordBasedFileManager::instance().openFile(rightParName, rightHandle);
    PageNum leftParSize = leftHandle.getNumberOfPages(), rightParSize = rightHandle.getNumberOfPages();
    RecordBasedFileManager::instance().closeFile(leftHandle);
    RecordBasedFileManager::instance().closeFile(rightHandle);

    if (parScanHandle.isOccupied()) RecordBasedFileManager::instance().closeFile(parScanHandle);
    parScanner.close();
    GHJoin::clearHashTable(records);

    // create in-memory hash table for smaller partition, set up scanner for larger partition
    FileHandle hashHandle;
    if (leftParSize <= rightParSize) {
        RecordBasedFileManager::instance().openFile(leftParName, hashHandle);
        GHJoin::createHashTable(hashHandle, leftDescriptor, leftCondAttrIndex, records);
        RecordBasedFileManager::instance().closeFile(hashHandle);
        RecordBasedFileManager::instance().openFile(rightParName, parScanHandle);
        RecordBasedFileManager::instance().scan(parScanHandle, rightDescriptor, "", NO_OP, nullptr, rightDesNames, parScanner);
        curScannedPar = "right";
    } else {
        RecordBasedFileManager::instance().openFile(rightParName, hashHandle);
        GHJoin::createHashTable(hashHandle, rightDescriptor, rightCondAttrIndex, records);
        RecordBasedFileManager::instance().closeFile(hashHandle);
        RecordBasedFileManager::instance().openFile(leftParName, parScanHandle);
        RecordBasedFileManager::instance().scan(parScanHandle, leftDescriptor, "", NO_OP, nullptr, leftDesNames, parScanner);
        curScannedPar = "left";
    }
}

RC GHJoin::getNextTuple(void *data) {
    if (curParID >= numPartitions) {
        return QE_EOF;
    }

    // get next record from scanning partition
    getNextScanRecord:
    RID rid;
    while (parScanner.getNextRecord(rid, data) == RBFM_EOF) {
        // current partitions end, read next partition
        curParID++;
        // already iterated all partitions, join ends
        if (curParID >= numPartitions) {
            return QE_EOF;
        }
        // load next partitions
        loadNextPartitions();
    }

    // find matches in hash table
    AttrLength attrLength;
    std::string attrStr;
    RecordSize recordSize;
    if (curScannedPar == "left") {
        Record r(leftDescriptor, data);
        void *attrData = r.getFieldValue(leftCondAttrIndex, attrLength);
        attrStr = Record::getAttrString(attrData, condAttrType, attrLength);
        free(attrData);
        recordSize = r.getSize();
    } else {
        Record r(rightDescriptor, data);
        void *attrData = r.getFieldValue(rightCondAttrIndex, attrLength);
        attrStr = Record::getAttrString(attrData, condAttrType, attrLength);
        free(attrData);
        recordSize = r.getSize();
    }
    if (records.count(attrStr) != 0) {  // has match
        void *matchedData = records[attrStr][0];
        void *scannedData = malloc(recordSize);
        memcpy(scannedData, data, recordSize);
        if (curScannedPar == "left") {
            Iterator::concatenateRecords(scannedData, matchedData, leftDescriptor, rightDescriptor, data);
        } else {
            Iterator::concatenateRecords(matchedData, scannedData, leftDescriptor, rightDescriptor, data);
        }
        records[attrStr].erase(records[attrStr].begin());
        free(matchedData);
        free(scannedData);
        if (records[attrStr].empty()) {
            records.erase(attrStr);
        }
    } else {  // no match
        goto getNextScanRecord;
    }

    return 0;
}

void GHJoin::getAttributes(std::vector<Attribute> &attrs) const {
    attrs = this->concatenateDescriptor;
}

GHJoin::~GHJoin() {
    GHJoin::clearHashTable(records);
    if (parScanHandle.isOccupied()) {
        RecordBasedFileManager::instance().closeFile(parScanHandle);
    }
    parScanner.close();
    // delete partition files
    for (auto i = 0; i < numPartitions; i++) {
        std::string parName = GHJoin::getPartitionName("left", i, curJoinID);
        RecordBasedFileManager::instance().destroyFile(parName);
        parName = GHJoin::getPartitionName("right", i, curJoinID);
        RecordBasedFileManager::instance().destroyFile(parName);
    }
}


Aggregate::Aggregate(Iterator *input, const Attribute &aggAttr, AggregateOp op) {
    this->input = input;
    this->aggAttr = aggAttr;
    this->op = op;
    this->input->getAttributes(this->inputDescriptor);
    this->returnedVal = 0;
    this->count = 0;
    eof = false;
    groupAttr.type = TypeNull;
    groupAttrIndex = inputDescriptor.size();
    aggAttrIndex = inputDescriptor.size();
    AttrType tmpType;
    Record::findAttrInDescriptor(inputDescriptor, aggAttr.name, aggAttrIndex, tmpType);
    assert(tmpType == aggAttr.type);
}

Aggregate::Aggregate(Iterator *input, const Attribute &aggAttr, const Attribute &groupAttr, AggregateOp op)
                    : Aggregate(input, aggAttr, op) {
    this->groupAttr = groupAttr;
    AttrType tmpType;
    Record::findAttrInDescriptor(inputDescriptor, groupAttr.name, groupAttrIndex, tmpType);
    assert(tmpType == groupAttr.type);
}

void Aggregate::getNormalOpResult() {
    void* scanData = malloc(TUPLE_TMP_SIZE);
    while (input->getNextTuple(scanData) != QE_EOF) {
        Record record(inputDescriptor, scanData);
        AttrLength attrLength;
        void* fieldValue = record.getFieldValue(aggAttrIndex, attrLength);
        if (fieldValue == nullptr) continue;
        int floatVal;
        if (aggAttr.type == TypeInt) {
            int intVal;
            memcpy(&intVal, (char *) fieldValue, sizeof(int));
            floatVal = intVal;
        }
        else if (aggAttr.type == TypeReal) {
            memcpy(&floatVal, (char *) fieldValue, sizeof(int));
        }
        else {
            std::string error = "unsupported aggregation attr type: " + std::to_string(aggAttr.type);
            throw std::invalid_argument(error);
        }
        if (op == MIN) {
            if (count == 0 || returnedVal > floatVal) {
                returnedVal = floatVal;
            }
        } else if (op == MAX) {
            if (count == 0 || returnedVal < floatVal) {
                returnedVal = floatVal;
            }
        } else if (op == COUNT) {
            returnedVal++;
        } else if (op == SUM || op == AVG) {
            returnedVal += floatVal;
        } else {
            std::string error = "unsupported aggregation type: " + std::to_string(op);
            throw std::invalid_argument(error);
        }
        count++;
        free(fieldValue);
    }

    free(scanData);

    if (op == AVG) {
        returnedVal = count == 0 ? 0 : returnedVal / count;
    }
}

void Aggregate::getGroupResult() {
    void *data = malloc(TUPLE_TMP_SIZE);
    AttrLength aggAttrLength, groupAttrLength;
    while (input->getNextTuple(data) != QE_EOF) {
        Record r(inputDescriptor, data);
        void *groupAttrData = r.getFieldValue(groupAttrIndex, groupAttrLength);
        if (groupAttrData == nullptr) continue;
        void *aggAttrData = r.getFieldValue(aggAttrIndex, aggAttrLength);
        if (aggAttrData == nullptr) {
            free(groupAttrData);
            continue;
        }

        count++;

        std::string key = Record::getAttrString(groupAttrData, groupAttr.type, groupAttrLength);
        if (groups.count(key) == 0) {
            if (op == MIN) {
                groups.insert(std::make_pair(key, std::numeric_limits<float>::max()));
            } else if (op == MAX) {
                groups.insert(std::make_pair(key, std::numeric_limits<float>::min()));
            } else if (op == SUM || op == COUNT) {
                groups.insert(std::make_pair(key, 0));
            } else if (op == AVG) {
                groups.insert(std::make_pair(key, 0));
                groupCount.insert(std::make_pair(key, 0));
            } else {
                throw std::invalid_argument("unsupported op type.");
            }
        }

        float aggAttrVal;
        if (aggAttr.type == TypeInt) {
            int val;
            memcpy(&val, aggAttrData, sizeof(int));
            aggAttrVal = val;
        } else if (aggAttr.type == TypeReal) {
            memcpy(&aggAttrVal, aggAttrData, sizeof(float));
        } else {
            throw std::invalid_argument("unsupported agg attr type.");
        }

        if (op == MIN) {
            if (groups[key] > aggAttrVal) {
                groups[key] = aggAttrVal;
            }
        } else if (op == MAX) {
            if (groups[key] < aggAttrVal) {
                groups[key] = aggAttrVal;
            }
        } else if (op == SUM) {
            groups[key] += aggAttrVal;
        } else if (op == COUNT) {
            groups[key]++;
        } else if (op == AVG) {
            groups[key] += aggAttrVal;
            groupCount[key]++;
        }

        free(groupAttrData);
        free(aggAttrData);
    }
    free(data);

    if (op == AVG) {
        for (auto & group : groups) {
            std::string key = group.first;
            float sum = group.second;
            int gCount = groupCount[key];
            group.second = gCount == 0 ? 0 : sum / gCount;
        }
    }
}

RC Aggregate::getNextTuple(void *data) {
    if (eof) {
        return QE_EOF;
    }

    if (groupAttr.type == TypeNull) {
        getNormalOpResult();
    } else {
        if (groups.empty()) {
            getGroupResult();
        }
    }

    if (count == 0) {
        eof = true;
        return QE_EOF;
    } else if (groupAttr.type == TypeNull) {
        auto* nullPointer = (unsigned char*) data;
        nullPointer[0] = 0;
        memcpy((char *) data + 1, &returnedVal, sizeof(float));
        eof = true;
        return 0;
    } else {
        if (groups.empty()) {
            eof = true;
            return QE_EOF;
        }
        auto* nullPointer = (unsigned char *) data;
        nullPointer[0] = 0;
        std::string key = groups.begin()->first;
        float aggVal = groups.begin()->second;
        if (op == MIN && aggVal == std::numeric_limits<float>::max()) {
            aggVal = 0;
        } else if (op == MAX && aggVal == std::numeric_limits<float>::min()) {
            aggVal = 0;
        }
        // transform back key and copy to data
        unsigned offset = 1;
        if (groupAttr.type == TypeInt) {
            int intKey = std::stoi(key);
            memcpy((char *) data + offset, &intKey, sizeof(int));
            offset += sizeof(int);
        } else if (groupAttr.type == TypeReal) {
            float floatKey = std::stof(key);
            memcpy((char *) data + offset, &floatKey, sizeof(float));
            offset += sizeof(float);
        } else if (groupAttr.type == TypeVarChar) {
            unsigned charLength = key.size();
            memcpy((char *) data + offset, &charLength, 4);
            offset += 4;
            for (auto &c: key) {
                memcpy((char *) data + offset, &c, sizeof(char));
                offset += sizeof(char);
            }
        }
        // copy agg value
        memcpy((char *) data + offset, &aggVal, sizeof(float));
        // pop key
        groups.erase(groups.begin());
        return 0;
    }
}

void Aggregate::getAttributes(std::vector<Attribute> &attrs) const {
    std::string attrName;
    switch (op) {
        case MIN: attrName += "MIN"; break;
        case MAX: attrName += "MAX"; break;
        case COUNT: attrName += "COUNT"; break;
        case SUM: attrName += "SUM"; break;
        case AVG: attrName += "AVG"; break;
        default:
            std::string error = "unsupported aggregation type: " + std::to_string(op);
            throw std::invalid_argument(error);
    }
    attrName += "(" + aggAttr.name + ")";
    Attribute attribute;
    attribute.name = attrName;
    attribute.type = TypeReal;
    attribute.length = (AttrLength) 4;
    attrs.push_back(attribute);
}

void BNLJoin::clearBlock(std::map<std::string, std::vector<void *>> &map) {
    for (auto & it : map) {
        for (auto pointer: it.second) {
            free(pointer);
        }
    }
    map.clear();
}

std::string BNLJoin::getRightKey() {
    Record r(rightDescriptor, rightData);
    AttrLength rightAttrDataLength;
    void *rightAttrData = r.getFieldValue(rightConditionAttrIndex, rightAttrDataLength);
    std::string rightAttrStr = Record::getAttrString(rightAttrData, conditionAttrType, rightAttrDataLength);
    free(rightAttrData);
    return rightAttrStr;
}

RC BNLJoin::getNextTuple(void *data) {
    if (leftInputFinish) return QE_EOF;
    // get next right record
    while (curLRIndex == -1) {
        rightInputFinish = rightIn->getNextTuple(rightData);
        // one round of right input finished, reset right input and get next right data
        if (rightInputFinish == QE_EOF) {
            rightIn->setIterator();
            rightInputFinish = rightIn->getNextTuple(rightData);
            // read new left block
            BNLJoin::clearBlock(leftRecords);
            leftInputFinish = BNLJoin::readBlock(leftIn, leftRecords, condition.lhsAttr, conditionAttrType, numPages);
        }
        // empty right input
        if (rightInputFinish == QE_EOF) {
            leftInputFinish = QE_EOF;
            return QE_EOF;
        }
        // left input reaches end, join ends
        if (leftInputFinish == QE_EOF) {
            return QE_EOF;
        }
        // see if current right data has match in the block
        std::string rightKey = getRightKey();
        // has match
        if (leftRecords.count(rightKey) != 0) {
            curLRIndex = 0;
        }
    }

    std::string rightKey = getRightKey();
    void *leftData = leftRecords[rightKey][curLRIndex];
    Iterator::concatenateRecords(leftData, rightData, leftDescriptor, rightDescriptor, data);
    curLRIndex++;
    if (curLRIndex == leftRecords[rightKey].size()) {
        curLRIndex = -1;
    }
    return 0;
}

void BNLJoin::getAttributes(std::vector<Attribute> &attrs) const {
    attrs = concatenateDescriptor;
}

BNLJoin::~BNLJoin() {
    if (rightData != nullptr) free(rightData);
    BNLJoin::clearBlock(leftRecords);
}

