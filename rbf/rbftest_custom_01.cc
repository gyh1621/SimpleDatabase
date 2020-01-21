//
// Created by gyh on 2020/1/16.
//

#include "pfm.h"
#include "test_util.h"

int RBFTest_Custom_1(PagedFileManager &pfm) {
    // Functions Tested:
    // 1. Open a file and passed to a file handler which is already a handler for some file
    // 2. Create a file with a name that is already for other files.
    // 3. Test inserting FSP and page number mapping
    // 4. Test finding page's free space in FSP
    std::cout << std::endl << "***** In RBF Test Custom Case 01 *****" << std::endl;

    RC rc;
    std::string fileName1 = "custom_test1";
    std::string fileName2 = "custom_test2";

    // Create files named "custom_test1" and "custom_test2"
    rc = pfm.createFile(fileName1);
    assert(rc == success && "Creating the file failed.");

    rc = createFileShouldSucceed(fileName1);
    assert(rc == success && "Creating the file failed.");

    rc = pfm.createFile(fileName2);
    assert(rc == success && "Creating the file failed.");

    rc = createFileShouldSucceed(fileName2);
    assert(rc == success && "Creating the file failed.");

    // test create file with same name;
    rc = pfm.createFile(fileName1);
    assert(rc != success && "Creating a file with name already exists should fail.");

    // Open the file
    FileHandle fileHandle;
    rc = pfm.openFile(fileName1, fileHandle);
    assert(rc == success && "Opening the file should not fail.");

    // Open the same file again use the same handler
    rc = pfm.openFile(fileName2, fileHandle);
    assert(rc != success && "Opening a file using a handler which is already a handler for some file should failed");

    // Insert 5000 pages and testing FSP inserting
    void *data = malloc(PAGE_SIZE);
    for (int i = 0; i < 5000; i++) {
        fileHandle.appendPage(data);
    }
    assert(fileHandle.getNumberOfPages() == 5000 && "Should be 5000 data pages in total");
    assert(fileHandle.getActualNumberOfPages() == 5000 + 1 + 3 && "Should be 5004 pages in total.");
    assert(fileHandle.changeToActualPageNum(4095) == 4098 && "Mapping to actual page number failed.");

    // test finding a page's free space in fsp
    PageNum fsp, pageIndex;
    PageNum pageNum = 1;
    fileHandle.getFSPofPage(pageNum, fsp, pageIndex);
    std::cout << fsp << "\t" << pageIndex << std::endl;
    assert(fsp == 1 && pageIndex == 1 && "finding fsp failed");
    pageNum = 4098;
    fileHandle.getFSPofPage(pageNum, fsp, pageIndex);
    std::cout << fsp << "\t" << pageIndex << std::endl;
    assert(fsp == 4099 && pageIndex == 2 && "finding fsp failed");

    // test read free space
    for (PageNum pageNum = 0; pageNum < fileHandle.getNumberOfPages(); pageNum++) {
        PageFreeSpace freePageSpace = fileHandle.getFreeSpaceOfPage(pageNum);
        assert(freePageSpace == 0 && "free page space should be 0 now");
    }

    // test write
    fileHandle.updateFreeSpaceOfPage(1000, 3000);
    assert(fileHandle.getFreeSpaceOfPage(1000) == 3000 && "free page space should be 3000 now");
    fileHandle.updateFreeSpaceOfPage(4999, 10);
    assert(fileHandle.getFreeSpaceOfPage(4999) == 10 && "free page space should be 1000 now");
    fileHandle.updateFreeSpaceOfPage(4999, 100);
    assert(fileHandle.getFreeSpaceOfPage(4999) == 100 && "free page space should be 1000 now");

    free(data);
    fileHandle.releaseHandle();

    std::cout << "RBF Test Custom Case 01 Finished! The result will not be examined :)" << std::endl << std::endl;

    return 0;
}

int main() {
    // To test the functionality of the paged file manager
    PagedFileManager &pfm = PagedFileManager::instance();

    // Remove files that might be created by previous test run
    remove("custom_test1");
    remove("custom_test2");

    return RBFTest_Custom_1(pfm);
}