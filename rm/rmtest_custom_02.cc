#include "rm_test_util.h"

RC TEST_RM_CUSTOM_02()
{
    // Functions Tested:
    // 1. create catalog twice
    // 2. delete tables not exist
    // 3. manually operate system tables
    std::cout <<std::endl << "***** In RM Custom Test Case 02 *****" <<std::endl;

    rm.deleteCatalog();
    RC rc = rm.createCatalog();
    assert(rc == 0);
    rc = rm.createCatalog();
    assert(rc != 0);

    rc = rm.deleteTable("tabletabletable");
    assert(rc != 0);

    rc = rm.deleteCatalog();
    assert(rc == 0);

    std::vector<Attribute> attrs;
    Attribute attr;
    attr.name = "arg1";
    attr.type = TypeNull;
    attr.length = 0;
    attrs.push_back(attr);
    rc = rm.createTable(SYSTABLE, attrs);
    assert(rc != 0);

    rc = rm.createCatalog();
    assert(rc == 0);

    void *data = malloc(100);
    RID rid;
    rc = rm.insertTuple(SYSTABLE, data, rid);
    assert(rc != 0);
    rc = rm.updateTuple(SYSTABLE, data, rid);
    assert(rc != 0);
    free(data);

    std::cout << "***** Custom Test Case 02 Finished. *****" <<std::endl;
    return 0;
}

int main()
{
    return TEST_RM_CUSTOM_02();
}

