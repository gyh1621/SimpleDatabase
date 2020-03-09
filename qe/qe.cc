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

