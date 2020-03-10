#include <sstream>
#include <iostream>
#include "qe.h"

bool Iterator::isAttrDataNull(const void *data) {
    bool nullBit = *(unsigned char *) ((char *) data) & ((unsigned) 1 << (unsigned) 7);
    return nullBit;
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
    this->concatenateDescriptor.clear();
    for (const auto & attr : this->leftDescriptor) {
        concatenateDescriptor.push_back(attr);
    }
    for (const auto & attr : this->rightDescriptor) {
        concatenateDescriptor.push_back(attr);
    }

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

    INLJoin::concatenate(leftData, rightData, leftDescriptor, rightDescriptor, data);
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
    AttrLength attrLength;
    switch (descriptor[i].type) {
        case TypeVarChar:
            memcpy(&attrLength, (char *) key + 1, sizeof(AttrLength));
            attrLength += sizeof(int);
            break;
        case TypeReal:
            attrLength = sizeof(float);
            break;
        case TypeInt:
            attrLength = sizeof(int);
            break;
        default: throw std::invalid_argument("unknown attr type.");
    }
    void* returnedKey = malloc(attrLength);
    memcpy((char *) returnedKey, (char *) key + 1, attrLength);
    free(key);
    return returnedKey;
}

void INLJoin::concatenate(const void *leftData, const void *rightData, const std::vector<Attribute> &leftDescriptor,
                          const std::vector<Attribute> &rightDescriptor, void *data) {
    auto newSize = leftDescriptor.size() + rightDescriptor.size();
    // init null indicators
    auto nullPointerSize = Record::getNullIndicatorSize(newSize);
    auto* nullPointer = (unsigned char*)data;
    for (int i = 0; i < nullPointerSize; i++) nullPointer[i] = 0;

    Record leftRecord(leftDescriptor, leftData);
    Record rightRecord(rightDescriptor, rightData);
    void* fieldValue = malloc(TUPLE_TMP_SIZE);
    int dataOffset = nullPointerSize;
    for (int i = 0; i < leftDescriptor.size(); i++) {
        leftRecord.readAttr(leftDescriptor, leftDescriptor[i].name, fieldValue);
        if(isAttrDataNull(fieldValue)) {
            int byteNum = i / 8;
            nullPointer[byteNum] |= ((char) 1) << (8 - (i % 8) - 1);
            continue;
        }
        switch (leftDescriptor[i].type) {
            case TypeVarChar:
                AttrLength length;
                memcpy(&length, (char *) fieldValue + 1, sizeof(AttrLength));
                memcpy((char *) data + dataOffset, (char *) fieldValue + 1, length + sizeof(AttrLength));
                dataOffset += length + sizeof(AttrLength);
                break;
            case TypeInt:
                memcpy((char *) data + dataOffset, (char  *) fieldValue + 1, sizeof(int));
                dataOffset += sizeof(int);
                break;
            case TypeReal:
                memcpy((char *) data + dataOffset, (char  *) fieldValue + 1, sizeof(float));
                dataOffset += sizeof(float);
                break;
            default: throw std::invalid_argument("unknown attr type.");
        }
    }

    for (int i = 0; i < rightDescriptor.size(); i++) {
        rightRecord.readAttr(rightDescriptor, rightDescriptor[i].name, fieldValue);
        if(isAttrDataNull(fieldValue)) {
            int byteNum = (i + leftDescriptor.size()) / 8;
            nullPointer[byteNum] |= ((char) 1) << (8 - ((i + leftDescriptor.size()) % 8) - 1);
            continue;
        }
        switch (rightDescriptor[i].type) {
            case TypeVarChar:
                AttrLength length;
                memcpy(&length, (char *) fieldValue + 1, sizeof(AttrLength));
                memcpy((char *) data + dataOffset, (char *) fieldValue + 1, length + sizeof(AttrLength));
                dataOffset += length + sizeof(AttrLength);
                break;
            case TypeInt:
                memcpy((char *) data + dataOffset, (char  *) fieldValue + 1, sizeof(int));
                dataOffset += sizeof(int);
                break;
            case TypeReal:
                memcpy((char *) data + dataOffset, (char  *) fieldValue + 1, sizeof(float));
                dataOffset += sizeof(float);
                break;
            default: throw std::invalid_argument("unknown attr type.");
        }
    }
    free(fieldValue);
}

void INLJoin::getAttributes(std::vector<Attribute> &attrs) const {
    attrs =  this->concatenateDescriptor;
}