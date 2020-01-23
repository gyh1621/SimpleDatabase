#include <fstream>
#include <cstring>
#include <cassert>
#include "page.h"
#include "record.h"

#ifndef _pfm_h_
#define _pfm_h_

typedef int RC;

typedef unsigned Counter;  // r/w/a counter

#include <string>

class FileHandle;


class PagedFileManager {
public:
    static PagedFileManager &instance();                                // Access to the _pf_manager instance

    RC createFile(const std::string &fileName);                         // Create a new file
    RC destroyFile(const std::string &fileName);                        // Destroy a file
    RC openFile(const std::string &fileName, FileHandle &fileHandle);   // Open a file
    RC closeFile(FileHandle &fileHandle);                               // Close a file

protected:
    PagedFileManager();                                                 // Prevent construction
    ~PagedFileManager();                                                // Prevent unwanted destruction
    PagedFileManager(const PagedFileManager &);                         // Prevent construction by copying
    PagedFileManager &operator=(const PagedFileManager &);              // Prevent assignment

};

class FileHandle {
private:
    std::fstream *handle;

    void *fspData;
    FreeSpacePage curFSP;
    PageNum curFSPNum;

    /* get current file's size of bytes
     *
     * Won't check if a file is open
     *
     * */
    std::streampos getFileSize() noexcept;

    /* write counters and page number to a hidden page
     *
     * Hidden page format:
     * ┌───────┬──────────────────┬───────────────────┬────────────────────┬─────────────┬────────────────┐
     * │ bool  │ unsigned         │ unsigned          │ unsigned           │  unsigned   │    unsigned    │
     * ├───────┼──────────────────┼───────────────────┼────────────────────┼─────────────┼────────────────┤
     * │IsInit │ readPageCounter  │ writePageCounter  │ appendPageCounter  │ pageNumber  │ dataPageNumber │
     * └───────┴──────────────────┴───────────────────┴────────────────────┴─────────────┴────────────────┘
     *
     * Return:
     *  0: success
     *  1: fail, currently only when no file is open
     */
    RC writeHiddenPage();

    /* read counters from the hidden page
     * Return:
     *  0: success, all counters updated from the file
     *  1: fail, file not contain a hidden page
     */
    RC readHiddenPage();

public:
    // variables to keep the counter for each operation
    Counter readPageCounter;
    Counter writePageCounter;
    Counter appendPageCounter;

    // page number
    PageNum totalPageNum;
    // data page number
    PageNum dataPageNum;

    FileHandle();                                                       // Default constructor
    ~FileHandle();                                                      // Destructor

    /* Set file handle
     * Return:
     *  0: set successfully
     *  1: a file is already opened
    */
    RC setHandle(std::fstream *f);

    /* Release file handle
     * Return:
     *  0: release successfully
     *  1: no file is opened
     */
    RC releaseHandle();

    /* Return handler status */
    bool isOccupied();

    /* Map data page number to actual page number
     *
     * when page size is 4096:
     *                                     |             2048 pages            |      |           2048 pages         |
     * actual page number:      0       1  |   2                        2049   | 2050 |   2051                4098   |
     * pages:              HiddenPage, FSP | DataPage, ..., ..., ..., DataPage | FSP  | DataPage, ..., ..., DataPage |
     * data page number:                   |   0                        2047   |      |   2048                4095   |
     *
     * Actual Page Number = (Data Page Number + 1) + ceil((Data Page Number + 1) / (float) PAGE_SIZE)
     *
     */
    static PageNum changeToActualPageNum(PageNum dataPageNum);

    /* Get a specific page
     * Return:
     *  0: success
     *  -1: invalid page number
     */
    RC readPage(PageNum pageNum, void *data, bool actual=false);
    /* Write a specific page
     * Return:
     *  0: success
     *  -1: invalid
     */
    RC writePage(PageNum pageNum, const void *data, bool actual=false);
    /* here pageNum is always dataPageNum */
    RC writePage(PageNum pageNum, PageFreeSpace freeSpace, const void *data);

    RC appendPage(const void *data, bool dataPage=true);                // Append a specific page
    RC appendPage(PageFreeSpace freeSpace, const void *data);           // Append a data page with free space specified
    int getNumberOfPages();                                             // Get the number of data pages in the file
    int getActualNumberOfPages();                                       // Get total number of pages
    RC collectCounterValues(Counter &readPageCount, Counter &writePageCount,
                            Counter &appendPageCount);                 // Put current counter values into variables

    /* pageNum below is always the data page number, starts from 0 */

    static void getFSPofPage(const PageNum &dataPageNum, PageNum &fspNum, PageNum &pageIndex);
    /* update current FreeSpacePage member
     * Return:
     *  0: updated
     *  -1: not updated
     */
    RC updateCurFSP(const PageNum &fspNum);
    PageFreeSpace getFreeSpaceOfPage(PageNum dataPageNum);
    void updateFreeSpaceOfPage(PageNum dataPageNum, PageFreeSpace freePageSpace);
};


#endif