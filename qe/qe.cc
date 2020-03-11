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
        memcpy((char *) data + dataOffset, fieldValue, attrLength);
        dataOffset += attrLength;
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
        memcpy((char *) data + dataOffset, fieldValue, attrLength);
        dataOffset += attrLength;
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
        std::string attrVal = Record::getString((char *) aData + 1, attrType, attrLength);

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

void INLJoin::getAttributes(std::vector<Attribute> &attrs) const {
    attrs =  this->concatenateDescriptor;
}

Aggregate::Aggregate(Iterator *input, const Attribute &aggAttr, AggregateOp op) {
    this->input = input;
    this->aggAttr = aggAttr;
    this->op = op;
    this->input->getAttributes(this->inputDescriptor);
    this->returnedVal = 0;
    this->count = 0;
}

RC Aggregate::getNextTuple(void *data) {
    void* scanData = malloc(TUPLE_TMP_SIZE);
    while (input->getNextTuple(scanData) != QE_EOF) {
        Record record(inputDescriptor, scanData);
        AttrLength attrLength;
        int i;
        for (i = 0; i < inputDescriptor.size(); i++) {
            if (inputDescriptor[i].name == aggAttr.name) {
                break;
            }
        }
        void* fieldValue = record.getFieldValue(i, attrLength);
        if (fieldValue == nullptr) continue;
        if (aggAttr.type == TypeInt) {
            int intVal;
            memcpy(&intVal, (char *) fieldValue, sizeof(int));
            if (op == MIN) {
                if (count == 0 || returnedVal > intVal) {
                    returnedVal = intVal;
                }
            } else if (op == MAX) {
                if (count == 0 || returnedVal < intVal) {
                    returnedVal = intVal;
                }
            } else if (op == COUNT) {
                returnedVal++;
            } else if (op == SUM || op == AVG) {
                returnedVal += intVal;
            }
            count++;
        }
        if (aggAttr.type == TypeReal) {
            int floatVal;
            memcpy(&floatVal, (char *) fieldValue, sizeof(int));
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
            }
            count++;
        }
        free(fieldValue);
    }

    free(scanData);

    if (count == 0) {
        return QE_EOF;
    }

    if (op == AVG) {
        returnedVal /= count;
    }
    count = 0;

    auto* nullPointer = (unsigned char*)data;
    nullPointer[0] = 0;
    memcpy((char *) data + 1, &returnedVal, sizeof(float));
    return 0;
}

void Aggregate::getAttributes(std::vector<Attribute> &attrs) const {
    std::string attrName;
    switch (op) {
        case MIN: attrName += "MIN"; break;
        case MAX: attrName += "MAX"; break;
        case COUNT: attrName += "COUNT"; break;
        case SUM: attrName += "SUM"; break;
        case AVG: attrName += "AVG"; break;
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
    std::string rightAttrStr = Record::getString(rightAttrData, conditionAttrType, rightAttrDataLength);
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

