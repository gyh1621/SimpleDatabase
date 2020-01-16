//
// Created by gyh on 2020/1/16.
//

#include "pfm.h"
#include "test_util.h"

int RBFTest_Custom_1(PagedFileManager &pfm) {
    // Functions Tested:
    // 1. Open a file and passed to a file handler which is already a handler for some file
    std::cout << std::endl << "***** In RBF Test Custom Case 01 *****" << std::endl;

    RC rc;
    std::string fileName = "custom_test1";

    // Create a file named "custom_test1"
    rc = pfm.createFile(fileName);
    assert(rc == success && "Creating the file failed.");

    rc = createFileShouldSucceed(fileName);
    assert(rc == success && "Creating the file failed.");

    // Open the file
    FileHandle fileHandle;
    rc = pfm.openFile(fileName, fileHandle);
    assert(rc == success && "Opening the file should not fail.");

    // Open the same file again use the same handler
    rc = pfm.openFile(fileName, fileHandle);
    assert(rc != success && "Opening a file using a handler which is already a handler for some file should failed");

    std::cout << "RBF Test Custom Case 01 Finished! The result will not be examined :)." << std::endl << std::endl;
    return 0;
}

int main() {
    // To test the functionality of the paged file manager
    PagedFileManager &pfm = PagedFileManager::instance();

    // Remove files that might be created by previous test run
    remove("custom_test1");

    return RBFTest_Custom_1(pfm);
}