#include "qe_test_util.h"

RC qeCustomTest01() {

    // Test: BNLJoin with varchar

    std::cerr << std::endl << "***** In QE Test Case Custom 01 *****" << std::endl;

    RC rc = success;

    // Prepare the iterator and condition
    auto *leftIn = new TableScan(rm, "leftvarchar");
    auto *rightIn = new TableScan(rm, "rightvarchar");

    // create BNLJoin
    Condition cond;
    cond.lhsAttr = "leftvarchar.B";
    cond.op = EQ_OP;
    cond.bRhsIsAttr = true;
    cond.rhsAttr = "rightvarchar.B";
    unsigned numPages = 5;

    auto *bnlJoin = new BNLJoin(leftIn, rightIn, cond, numPages);

    // Set up condition
    Condition filterCond;
    filterCond.lhsAttr = "leftvarchar.A";
    filterCond.op = LE_OP;
    filterCond.bRhsIsAttr = false;
    Value value1{};
    value1.type = TypeInt;
    value1.data = malloc(bufSize);
    *(int *) value1.data = 45; // A[20-45], then B: a ... zzzzzzzzzzzzzzzzzzzzzzzzzz
    filterCond.rhsValue = value1;

    // Create Filter
    auto *filter = new Filter(bnlJoin, filterCond);

    // Set up condition
    Condition filterCond2;
    filterCond2.lhsAttr = "rightvarchar.C";
    filterCond2.op = LE_OP;
    filterCond2.bRhsIsAttr = false;
    Value value2{};
    value2.type = TypeReal;
    value2.data = malloc(bufSize);
    *(float *) value2.data = 35.0; // C[10.0-35.0], then B: a ... zzzzzzzzzzzzzzzzzzzzzzzzzz
    filterCond2.rhsValue = value2;

    // Create Filter
    auto *filter2 = new Filter(filter, filterCond2);

    int expectedResultCnt = 26;
    int actualResultCnt = 0;

    // Go over the data through iterator
    void *data = malloc(bufSize);
    bool nullBit;

    while (filter2->getNextTuple(data) != QE_EOF) {

        // At this point, partitions should be on disk.

        std::cerr << (actualResultCnt + 1) << " / " << expectedResultCnt << " tuples: ";
        int offset = 0;
        // is an attribute leftvarchar.A NULL?
        nullBit = *(unsigned char *) ((char *) data) & ((unsigned) 1 << (unsigned) 7);
        if (nullBit) {
            std::cerr << std::endl << "***** A returned value is not correct. *****" << std::endl;
            rc = fail;
            break;
        }

        // Print left.A
        std::cerr << "leftvarchar.A " << *(int *) ((char *) data + offset + 1);
        offset += sizeof(int);

        // is an attribute left.B NULL?
        nullBit = *(unsigned char *) ((char *) data) & ((unsigned) 1 << (unsigned) 6);
        if (nullBit) {
            std::cerr << std::endl << "***** A returned value is not correct. *****" << std::endl;
            rc = fail;
            break;
        }
        // Print left.B
        int length = *(int *) ((char *) data + offset + 1);
        offset += 4;

        char *b = (char *) malloc(100);
        memcpy(b, (char *) data + offset + 1, length);
        b[length] = '\0';
        offset += length;
        std::cerr << " B " << b;

        // is an attribute right.C NULL?
        nullBit = *(unsigned char *) ((char *) data) & ((unsigned) 1 << (unsigned) 4);
        if (nullBit) {
            std::cerr << std::endl << "***** A returned value is not correct. *****" << std::endl;
            rc = fail;
            break;
        }

        // skip rightvarchar.B
        offset = offset + 4 + length;

        // Print right.C
        std::cerr << "  rightvarchar.C " << *(float *) ((char *) data + offset + 1) << std::endl;

        memset(data, 0, bufSize);
        free(b);
        actualResultCnt++;

    }

    if (expectedResultCnt != actualResultCnt) {
        std::cerr << "***** The number of returned tuple: " << actualResultCnt << " is not correct. *****" << std::endl;
        rc = fail;
    }

    delete bnlJoin;
    delete filter;
    delete filter2;
    delete leftIn;
    delete rightIn;
    free(value1.data);
    free(value2.data);
    free(data);
    return rc;
}

int main() {

    // use table: "leftvarchar", "rightvarchar", created by qetest_04

    if (qeCustomTest01() != success) {
        std::cerr << "***** [FAIL] QE Test Case Custom 01 failed. *****" << std::endl;
        return fail;
    } else {
        std::cerr << "***** QE Test Case Custom 01 finished. *****" << std::endl;
        return success;
    }
}

