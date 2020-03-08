#include <sstream>
#include <iostream>
#include "qe.h"

bool Iterator::isAttrDataNull(const void *data) {
    bool nullBit = *(unsigned char *) ((char *) data) & ((unsigned) 1 << (unsigned) 7);
    return nullBit;
}

std::string Iterator::getAttributeName(const std::string &relAttrName) {
    std::istringstream s(relAttrName);
    std::string attrName;
    std::getline(s, attrName, '.');
    std::getline(s, attrName, '.');
    return attrName;
}

std::vector<Attribute> Iterator::getRecordDescriptor(const std::vector<Attribute> &attributes) {
    std::vector<Attribute> descriptor;
    for (const Attribute& attr: attributes) {
        Attribute newAttr = attr;
        newAttr.name = Iterator::getAttributeName(attr.name);
        descriptor.push_back(newAttr);
    }
    return descriptor;
}

Filter::Filter(Iterator *input, const Condition &condition) {
    this->input = input;
    this->condition = condition;
    // transform attribute name in condition
    this->condition.lhsAttr = Iterator::getAttributeName(this->condition.lhsAttr);
    if (this->condition.bRhsIsAttr) {
        this->condition.rhsAttr = Iterator::getAttributeName(this->condition.rhsAttr);
    }
    // get record descriptor
    std::vector<Attribute> attributes;
    this->input->getAttributes(attributes);
    this->recordDescriptor = Iterator::getRecordDescriptor(attributes);
}

RC Filter::getNextTuple(void *data) {
    RC rc = this->input->getNextTuple(data);
    if (rc == QE_EOF) {
        return rc;
    }
    if (condition.op == NO_OP) {
        return 0;
    }

    Record r(recordDescriptor, data);

    // read condition's left value
    void *leftData = malloc(PAGE_SIZE);
    if (leftData == nullptr) throw std::bad_alloc();
    r.readAttr(recordDescriptor, condition.lhsAttr, leftData);

    // read condition's right value
    void *rightData = malloc(PAGE_SIZE);
    if (rightData == nullptr) throw std::bad_alloc();
    if (condition.bRhsIsAttr) {
        // should not happen
        throw std::invalid_argument("filter condition's right is an attribute");
        //r.readAttr(recordDescriptor, condition.rhsAttr, rightData);
    } else {
        *((bool *) rightData) = false;
        switch (condition.rhsValue.type) {
            case TypeInt:
            case TypeReal:
                memcpy((char *) rightData + 1, condition.rhsValue.data, 4);
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
    } else {
        return getNextTuple(data);
    }
}

void Filter::getAttributes(std::vector<Attribute> &attrs) const {
    this->input->getAttributes(attrs);
}

