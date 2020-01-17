#include <fstream>

#ifndef _pfm_h_
#define _pfm_h_

typedef unsigned PageNum;
typedef int RC;

#define PAGE_SIZE 4096

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

    /* get current file's size of bytes
     *
     * Won't check if a file is open
     *
     * */
    std::streampos getFileSize() noexcept;

    /* write counters and page number to a hidden page
     *
     * Hidden page format:
     * char     unsigned int        unsigned int        unsigned int        unsigned int
     * IsInit   readPageCounter     writePageCounter    appendPageCounter   pageNumber
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
    unsigned readPageCounter;
    unsigned writePageCounter;
    unsigned appendPageCounter;

    // page number, don't count hidden page
    PageNum totalPage;

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

    /* Get a specific page
     * Return:
     *  0: success
     *  -1: invalid page number
     */
    RC readPage(int pageNum, void *data);
    RC writePage(int pageNum, const void *data);                    // Write a specific page

    RC appendPage(const void *data);                                    // Append a specific page
    unsigned getNumberOfPages();                                        // Get the number of pages in the file
    RC collectCounterValues(unsigned &readPageCount, unsigned &writePageCount,
                            unsigned &appendPageCount);                 // Put current counter values into variables
};

#endif