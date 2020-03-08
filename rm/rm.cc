#include "rm.h"
#include "../ix/ix.h"

RelationManager &RelationManager::instance() {
    static RelationManager _relation_manager = RelationManager();
    return _relation_manager;
}

RelationManager::RelationManager() {
    tableNumber = getTableNumbers();
}

RelationManager::~RelationManager() = default;

RelationManager::RelationManager(const RelationManager &) = default;

RelationManager &RelationManager::operator=(const RelationManager &) = default;

RC RelationManager::createCatalog() {
    RC rc;
    rc = RecordBasedFileManager::instance().createFile(SYSTABLE);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().createFile(SYSCOLTABLE);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().createFile(SYSIDXTABLE);
    if(rc != 0) return rc;
    std::vector<Attribute> tablesAttrs;
    std::vector<Attribute> columnsAttrs;
    std::vector<Attribute>  indexesAttrs;
    getSysTableAttributes(tablesAttrs);
    getSysColTableAttributes(columnsAttrs);
    getSysIdxTableAttributes(indexesAttrs);
    addMetaInfo(SYSTABLE, tablesAttrs);
    addMetaInfo(SYSCOLTABLE, columnsAttrs);
    addMetaInfo(SYSIDXTABLE, indexesAttrs);
    return rc;
}

RC RelationManager::deleteCatalog() {
    RC rc;
    rc = RecordBasedFileManager::instance().destroyFile(SYSTABLE);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().destroyFile(SYSCOLTABLE);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().destroyFile(SYSIDXTABLE);
    tableNumber = 0;
    return rc;
}

RC RelationManager::createTable(const std::string &tableName, const std::vector<Attribute> &attrs) {
    RC rc = 0;
    TableID id;
    std::string fileName;
    rc = getTableInfo(tableName, id, fileName);

    // table exists
    if(rc == 0) return -2;
    if(!isSysTable(tableName)){
        rc = RecordBasedFileManager::instance().createFile(tableName);
        if(rc != 0) return rc;
        addMetaInfo(tableName, attrs);
    }else{
        return -1;
    }
    return rc;
}


RC RelationManager::deleteTable(const std::string &tableName) {
    RC rc = 0;
    TableID id;
    std::string fileName;
    rc = getTableInfo(tableName, id, fileName);
    if(rc != 0) return rc;
    if(!isSysTable(tableName)){
        rc = RecordBasedFileManager::instance().destroyFile(fileName);
        assert(rc == 0);
        rc = deleteMetaInfo(tableName);
        assert(rc == 0);
    }else{
        return -1;
    }

    std::vector<Attribute> attrs;
    getAttributes(tableName, attrs);
    for (int i = 0; i < attrs.size(); i++) {
        destroyIndex(tableName, attrs[i].name);
    }
    return rc;
}

RC RelationManager::getAttributes(const std::string &tableName, std::vector<Attribute> &attrs) {
    if (tableName == SYSTABLE) {
        getSysTableAttributes(attrs);
        return 0;
    } else if (tableName == SYSCOLTABLE) {
        getSysColTableAttributes(attrs);
        return 0;
    }  else if (tableName == SYSIDXTABLE) {
        getSysIdxTableAttributes((attrs));
        return 0;
    }

    std::string tableFileName;
    TableID tableID;
    RC rc = getTableInfo(tableName, tableID, tableFileName);
    if (rc != 0) return rc;

    std::vector<Attribute> descriptor;
    getSysColTableAttributes(descriptor);
    std::vector<std::string> attrNames;
    getDescriptorString(descriptor, attrNames);

    RM_ScanIterator rmsi;
    rc = scan(SYSCOLTABLE, "table-id", EQ_OP, &tableID, attrNames, rmsi);
    assert(rc == 0);

    void *data = malloc(TUPLE_TMP_SIZE);
    RID rid;

    AttrLength attrLength;
    while (rmsi.getNextTuple(rid, data) != RM_EOF) {
        Record r(descriptor, data);
        Attribute attr;
        void *attrData = r.getFieldValue(1, attrLength);  // column-name
        attr.name = Record::getString(attrData, attrLength);
        free(attrData);
        attrData = r.getFieldValue(2, attrLength);  // column-type
        attr.type = *((AttrType *) attrData);
        free(attrData);
        attrData = r.getFieldValue(3, attrLength);  // column-length
        attr.length = *((AttrLength *) attrData);
        free(attrData);
        attrs.push_back(attr);
    }

    free(data);
    rmsi.close();

    return 0;
}

RC RelationManager::insertTuple(const std::string &tableName, const void *data, RID &rid) {
    return insertTuple(tableName, data, rid, false);
}

RC RelationManager::deleteTuple(const std::string &tableName, const RID &rid) {
    return deleteTuple(tableName, rid, false);
}

RC RelationManager::updateTuple(const std::string &tableName, const void *data, const RID &rid) {
    if(isSysTable(tableName)) return -1;
    FileHandle fileHandle;
    std::string fileName;
    TableID id;
    std::vector<Attribute> attr;
    getAttributes(tableName, attr);
    void* oldData = malloc(TUPLE_TMP_SIZE);
    RC rc;
    rc = getTableInfo(tableName, id, fileName);
    if(rc != 0) return -2;
    rc = RecordBasedFileManager::instance().openFile(fileName, fileHandle);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().readRecord(fileHandle, attr, rid, oldData);
    if(rc != 0) return rc;
    deleteIdxEntry(tableName, attr, oldData, rid);
    rc = RecordBasedFileManager::instance().updateRecord(fileHandle, attr, data, rid);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().closeFile(fileHandle);
    insertIdxEntry(tableName, attr, data, rid);
    free(oldData);
    return rc;
}

RC RelationManager::readTuple(const std::string &tableName, const RID &rid, void *data) {
    if(isSysTable(tableName)) return -1;
    FileHandle fileHandle;
    std::string fileName;
    TableID id;
    std::vector<Attribute> attr;
    getAttributes(tableName, attr);
    RC rc;
    rc = getTableInfo(tableName, id, fileName);
    if(rc != 0) return -2;
    rc = RecordBasedFileManager::instance().openFile(fileName, fileHandle);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().readRecord(fileHandle, attr, rid, data);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().closeFile(fileHandle);
    return rc;
}

RC RelationManager::printTuple(const std::vector<Attribute> &attrs, const void *data) {
    RC rc;
    rc = RecordBasedFileManager::instance().printRecord(attrs, data);
    return rc;
}

RC RelationManager::readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName,
                                  void *data) {
    if(isSysTable(tableName)) return -1;
    FileHandle fileHandle;
    TableID id;
    std::string fileName;
    std::vector<Attribute> attr;
    getAttributes(tableName, attr);
    RC rc;
    rc = getTableInfo(tableName, id, fileName);
    if(rc != 0) return -2;
    rc = RecordBasedFileManager::instance().openFile(fileName, fileHandle);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().readAttribute(fileHandle, attr, rid, attributeName, data);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().closeFile(fileHandle);
    return rc;
}

RC RelationManager::scan(const std::string &tableName,
                         const std::string &conditionAttribute,
                         const CompOp compOp,
                         const void *value,
                         const std::vector<std::string> &attributeNames,
                         RM_ScanIterator &rm_ScanIterator) {
    std::string tableFileName;
    TableID tableID;
    RC rc = getTableInfo(tableName, tableID, tableFileName);
    assert(rc == 0);
    std::vector<Attribute> descriptor;
    getAttributes(tableName, descriptor);
    rm_ScanIterator.setUp(tableFileName, conditionAttribute, compOp, value, attributeNames, descriptor);
    return 0;
}

RC RelationManager::insertTuple(const std::string &tableName, const void *data, RID &rid, bool sys) {
    if(!sys && isSysTable(tableName)) return -1;
    FileHandle fileHandle;
    TableID id;
    std::string fileName;
    std::vector<Attribute> attr;
    getAttributes(tableName, attr);
    RC rc;
    rc = getTableInfo(tableName, id, fileName);
    if(rc != 0) return -2;
    rc = RecordBasedFileManager::instance().openFile(fileName, fileHandle);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().insertRecord(fileHandle, attr, data, rid);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().closeFile(fileHandle);
    insertIdxEntry(tableName, attr, data, rid);
    return rc;
}

RC RelationManager::deleteTuple(const std::string &tableName, const RID &rid, bool sys) {
    if(!sys && isSysTable(tableName)) return -1;
    FileHandle fileHandle;
    TableID id;
    std::string fileName;
    std::vector<Attribute> attr;
    getAttributes(tableName, attr);
    void* data = malloc(TUPLE_TMP_SIZE);
    RC rc;
    rc = getTableInfo(tableName, id, fileName);
    if(rc != 0) return -2;
    rc = RecordBasedFileManager::instance().openFile(fileName, fileHandle);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().readRecord(fileHandle, attr, rid, data);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().deleteRecord(fileHandle, attr, rid);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().closeFile(fileHandle);
    deleteIdxEntry(tableName, attr, data, rid);
    free(data);
    return rc;
}

void RelationManager::getSysTableAttributes(std::vector<Attribute> &descriptor) {
    Attribute attribute;
    attribute.name = "table-id";
    attribute.type = TypeInt;
    attribute.length = (AttrLength) 4;
    descriptor.push_back(attribute);

    attribute.name = "table-name";
    attribute.type = TypeVarChar;
    attribute.length = (AttrLength) 50;
    descriptor.push_back(attribute);

    attribute.name = "file-name";
    attribute.type = TypeVarChar;
    attribute.length = (AttrLength) 50;
    descriptor.push_back(attribute);
}

void RelationManager::getSysColTableAttributes(std::vector<Attribute> &descriptor) {
    Attribute attribute;

    attribute.name = "table-id";
    attribute.type = TypeInt;
    attribute.length = (AttrLength) 4;
    descriptor.push_back(attribute);

    attribute.name = "column-name";
    attribute.type = TypeVarChar;
    attribute.length = (AttrLength) 50;
    descriptor.push_back(attribute);

    attribute.name = "column-type";
    attribute.type = TypeInt;
    attribute.length = (AttrLength) 4;
    descriptor.push_back(attribute);

    attribute.name = "column-length";
    attribute.type = TypeInt;
    attribute.length = (AttrLength) 4;
    descriptor.push_back(attribute);

    attribute.name = "column-position";
    attribute.type = TypeInt;
    attribute.length = (AttrLength) 4;
    descriptor.push_back(attribute);
}

void RelationManager::getSysIdxTableAttributes(std::vector<Attribute> &descriptor) {
    Attribute attribute;
    attribute.name = "index-name";
    attribute.type = TypeVarChar;
    attribute.length = (AttrLength) 50;
    descriptor.push_back(attribute);

    attribute.name = "file-name";
    attribute.type = TypeVarChar;
    attribute.length = (AttrLength) 50;
    descriptor.push_back(attribute);
}

void RelationManager::addMetaInfo(const std::string &tableName, const std::vector<Attribute> &descriptor) {
    tableNumber++;
    TableID id = tableNumber;
    addTablesInfo(tableName, id);
    addColumnsInfo(tableName, id, descriptor);
}

TableID RelationManager::getTableNumbers() {
    TableID count = 0;
    FileHandle fileHandle;
    RC rc = RecordBasedFileManager::instance().openFile(SYSTABLE, fileHandle);
    if (rc == -1) {
        // catalog not created
        return count;
    }
    assert(rc == 0 && "Open file failed");
    //PageNum pageNum = fileHandle.dataPageNum;
    //void* page = malloc(PAGE_SIZE);
    //if (page == nullptr) throw std::bad_alloc();
    //for(int i = 0; i < pageNum; i++){
    //    fileHandle.readPage(i, page);
    //    DataPage p(page);
//  //      count += p.getRecordNumber();
    //    count += p.getSlotNumber();
    //}
    //RecordBasedFileManager::instance().closeFile(fileHandle);
    RM_ScanIterator rmsi;
    RID rid;
    void* data = malloc(TUPLE_TMP_SIZE);
    if(data == nullptr) throw std::bad_alloc();
    std::vector<Attribute> descriptor;
    getSysTableAttributes(descriptor);
    std::vector<std::string> attrNames;
    getDescriptorString(descriptor, attrNames);
    rc = scan(SYSTABLE, "", NO_OP, NULL, attrNames, rmsi);
    assert(rc == 0);

    AttrLength attrLength;
    while(rmsi.getNextTuple(rid, data) != RM_EOF){
        Record record(descriptor, data);
        void *attrData = record.getFieldValue(0, attrLength);
        assert(attrData != nullptr);
        TableID newID = *((TableID *) attrData);
        count = std::max(newID, count);
        free(attrData);
    }
    free(data);
    rmsi.close();
    return count;
}

void RelationManager::addTablesInfo(const std::string &tableName, TableID id) {
    void* data = malloc(TUPLE_TMP_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    int nullPointerSize = static_cast<int>(ceil(3 / 8.0));
    auto* nullPointer = (unsigned char*)data;
    for(int i = 0; i < nullPointerSize; i++) nullPointer[i] = 0;

    // null pointer
    memcpy((char *) data, nullPointer, static_cast<size_t>(nullPointerSize));
    int dataOffset = nullPointerSize;
    // table-id
    memcpy((char *)data + dataOffset, &id, sizeof(TableID));
    // table-name
    dataOffset += sizeof(TableID);
    int length = static_cast<int>(tableName.size());
    memcpy((char *)data + dataOffset, &length, 4);
    dataOffset += 4;
    memcpy((char *) data + dataOffset, tableName.c_str(), static_cast<size_t>(length));
    dataOffset += length;
    //file-name;
    memcpy((char *)data + dataOffset, &length, sizeof(int));
    dataOffset += 4;
    memcpy((char *) data + dataOffset, tableName.c_str(), static_cast<size_t>(length));

    RID rid;
    RC rc = insertTuple(SYSTABLE, data, rid, true);
    assert(rc == 0);
    free(data);
}

void RelationManager::addColumnsInfo(const std::string &tableName, TableID id, const std::vector<Attribute> &descriptor) {
    std::vector<Attribute> colAttr;
    getSysColTableAttributes(colAttr);

    int nullPointerSize = static_cast<int>(ceil(5 / 8.0));
    void* data;
    int dataOffset;
    for(int i = 0; i < descriptor.size(); i++){
        data = malloc(TUPLE_TMP_SIZE);
        auto* nullPointer = (unsigned char*)data;
        for(int j = 0; j < nullPointerSize; j++) nullPointer[j] = 0;
        dataOffset = nullPointerSize;
        // id
        memcpy((char *)data + dataOffset, &id, sizeof(int));
        dataOffset += 4;
        // column-name
        int length = static_cast<int>(descriptor[i].name.size());
        memcpy((char *)data + dataOffset, &length, sizeof(int));
        dataOffset += 4;
        memcpy((char *) data + dataOffset, descriptor[i].name.c_str(), static_cast<size_t>(length));
        dataOffset += length;
        //column-type
        memcpy((char *)data + dataOffset, &descriptor[i].type, sizeof(AttrType));
        dataOffset += sizeof(AttrType);
        //column-length
        memcpy((char *)data + dataOffset, &descriptor[i].length, sizeof(AttrLength));
        dataOffset += sizeof(AttrLength);
        //column-position
        int position = i + 1;
        memcpy((char *)data + dataOffset, &position, sizeof(int));
        RID rid;
        RC rc = insertTuple(SYSCOLTABLE, data, rid, true);
        assert(rc == 0);
        free(data);
    }
}

RC RelationManager::deleteMetaInfo(const std::string &tableName) {
    TableID id;
    std::string fileName;
    RC rc = getTableInfo(tableName, id, fileName);
    if (rc != 0) {
        return rc;
    }

    RM_ScanIterator rmsi;
    RID rid;
    void* data = malloc(TUPLE_TMP_SIZE);
    if(data == nullptr) throw std::bad_alloc();

    // delete record in sys table
    std::vector<Attribute> descriptor;
    getSysTableAttributes(descriptor);
    std::vector<std::string> attrNames;
    getDescriptorString(descriptor, attrNames);
    void *tableNameData = createVarcharData(tableName);
    rc = scan(SYSTABLE, "table-name", EQ_OP, tableNameData, attrNames, rmsi);
    assert(rc == 0);
    if(rmsi.getNextTuple(rid, data) != RM_EOF){
        RC res = deleteTuple(SYSTABLE, rid, true);
        assert(res == 0 && "delete tuple failed");
    }
    descriptor.clear();
    attrNames.clear();
    free(tableNameData);
    rmsi.close();
    // delete record in sys column
    getSysColTableAttributes(descriptor);
    getDescriptorString(descriptor, attrNames);
    rc = scan(SYSCOLTABLE, "table-id", EQ_OP, &id, attrNames, rmsi);
    assert(rc == 0);
    while(rmsi.getNextTuple(rid, data) != RM_EOF){
        RC res = deleteTuple(SYSCOLTABLE, rid, true);
        assert(res == 0 && "delete tuple failed");
    }
    free(data);
    rmsi.close();

    return 0;
}

void RelationManager::getDescriptorString(const std::vector<Attribute> &descriptor, std::vector<std::string> &attrNames) {
    for(const auto & i : descriptor){
        attrNames.push_back(i.name);
    }
}

RC RelationManager::getTableInfo(const std::string &tableName, TableID &id, std::string &fileName) {
    if(tableName == SYSTABLE){
        id = 1;
        fileName = SYSTABLE;
        return 0;
    }
    if(tableName == SYSCOLTABLE){
        id = 2;
        fileName = SYSCOLTABLE;
        return 0;
    }
    if(tableName == SYSIDXTABLE){
        id = 3;
        fileName = SYSIDXTABLE;
        return 0;
    }
    RM_ScanIterator rmsi;
    RID rid;
    void* data = malloc(TUPLE_TMP_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    std::vector<Attribute> descriptor;
    getSysTableAttributes(descriptor);
    std::vector<std::string> attrNames;
    getDescriptorString(descriptor, attrNames);
    RC rc;
    void *tableNameData = createVarcharData(tableName);
    rc = scan(SYSTABLE, "table-name", EQ_OP, tableNameData, attrNames, rmsi);
    assert(rc == 0);
    if(rmsi.getNextTuple(rid, data) != RM_EOF){
        // 1 bit nullIndicator
        int offset = 1;
        // get id;
        memcpy(&id, (char *)data + offset, sizeof(int));
        offset += sizeof(int);
        int length;
        memcpy(&length, (char *)data + offset, sizeof(int));
        offset += sizeof(int) + length;
        memcpy(&length, (char *)data + offset, sizeof(int));
        offset += sizeof(int);
        char* varchar = new char[length];
        memcpy(varchar, (char *)data + offset, static_cast<size_t>(length));
        std::string s(varchar, static_cast<unsigned long>(length));
        delete[](varchar);
        //get fileHandle
        fileName = s;
        free(tableNameData);
        free(data);
        rmsi.close();
        return 0;
    }else{
        free(tableNameData);
        free(data);
        rmsi.close();
        return -2;
    }
}

bool RelationManager::isSysTable(const std::string &tableName) {
    return tableName == SYSTABLE || tableName == SYSCOLTABLE || tableName == SYSIDXTABLE;
}

void* RelationManager::createVarcharData(const std::string &str) {
    const char *chars = str.c_str();
    void *data = malloc(4 + str.length());
    if (data == nullptr) throw std::bad_alloc();
    auto length = static_cast<unsigned int>(str.length());
    memcpy(data, &length, 4);
    memcpy((char *) data + 4, chars, str.length());
    return data;
}

void RelationManager::insertIdxEntry(const std::string &tableName, const std::vector<Attribute> attrs, const void *data,
                                    const RID &rid) {
    void* key;
    AttrLength attrLength;
    std::string fileName;
    IXFileHandle ixFileHandle;
    for (int i = 0; i < attrs.size(); i++) {
        if (getIndexFileName(tableName, attrs[i], fileName) != 0) {
            continue;
        }
        Record record(attrs, data);
        key = record.getFieldValue(i, attrLength);
        IndexManager::instance().openFile(fileName, ixFileHandle);
        IndexManager::instance().insertEntry(ixFileHandle, attrs[i], key, rid);
        IndexManager::instance().closeFile(ixFileHandle);
        free(key);
    }
}

void RelationManager::deleteIdxEntry(const std::string &tableName, const std::vector<Attribute> attrs, const void *data,
                                    const RID &rid) {
    void* key;
    AttrLength attrLength;
    std::string fileName;
    IXFileHandle ixFileHandle;
    for (int i = 0; i < attrs.size(); i++) {
        if (getIndexFileName(tableName, attrs[i], fileName) != 0) {
            continue;
        }
        Record record(attrs, data);
        key = record.getFieldValue(i, attrLength);
        IndexManager::instance().openFile(fileName, ixFileHandle);
        IndexManager::instance().deleteEntry(ixFileHandle, attrs[i], key, rid);
        IndexManager::instance().closeFile(ixFileHandle);
        free(key);
    }
}

RC RelationManager::getIndexFileName(const std::string &tableName, const Attribute &attribute,
                                       std::string &fileName) {
    std::string indexName = tableName + "." + attribute.name;
    RM_ScanIterator rmsi;
    RID rid;
    void* data = malloc(TUPLE_TMP_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    std::vector<Attribute> descriptor;
    getSysIdxTableAttributes(descriptor);
    std::vector<std::string> attrNames;
    getDescriptorString(descriptor, attrNames);
    RC rc;
    void *indexNameData = createVarcharData(indexName);
    rc = scan(SYSIDXTABLE, "index-name", EQ_OP, indexNameData, attrNames, rmsi);
    assert(rc == 0);
    if(rmsi.getNextTuple(rid, data) != RM_EOF){
        // 1 bit nullIndicator
        int offset = 1;
        int length;
        memcpy(&length, (char *)data + offset, sizeof(int));
        offset += sizeof(int) + length;
        memcpy(&length, (char *)data + offset, sizeof(int));
        offset += sizeof(int);
        char* varchar = new char[length];
        memcpy(varchar, (char *)data + offset, static_cast<size_t>(length));
        std::string s(varchar, static_cast<unsigned long>(length));
        delete[](varchar);
        //get fileHandle
        fileName = s;
        free(indexNameData);
        free(data);
        rmsi.close();
        rc = 0;
    }else{
        free(indexNameData);
        free(data);
        rmsi.close();
        rc = 1;
    }
    return rc;
}

void RelationManager::addIndexInfo(const std::string &fileName) {
    void* data = malloc(TUPLE_TMP_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    int nullPointerSize = static_cast<int>(ceil(2 / 8.0));
    auto* nullPointer = (unsigned char*)data;
    for(int i = 0; i < nullPointerSize; i++) nullPointer[i] = 0;

    // null pointer
    memcpy((char *) data, nullPointer, static_cast<size_t>(nullPointerSize));
    int dataOffset = nullPointerSize;
    // index-name
    int length = static_cast<int>(fileName.size());
    memcpy((char *)data + dataOffset, &length, 4);
    dataOffset += 4;
    memcpy((char *) data + dataOffset, fileName.c_str(), static_cast<size_t>(length));
    dataOffset += length;
    //file-name;
    memcpy((char *)data + dataOffset, &length, sizeof(int));
    dataOffset += 4;
    memcpy((char *) data + dataOffset, fileName.c_str(), static_cast<size_t>(length));

    RID rid;
    RC rc = insertTuple(SYSIDXTABLE, data, rid, true);
    assert(rc == 0);
    free(data);
}

void RelationManager::deleteIndexInfo(const std::string fileName) {
    RC rc;
    RM_ScanIterator rmsi;
    RID rid;
    void* data = malloc(TUPLE_TMP_SIZE);
    if(data == nullptr) throw std::bad_alloc();

    // delete record in sys table
    std::vector<Attribute> descriptor;
    getSysIdxTableAttributes(descriptor);
    std::vector<std::string> attrNames;
    getDescriptorString(descriptor, attrNames);
    void *tableNameData = createVarcharData(fileName);
    rc = scan(SYSIDXTABLE, "index-name", EQ_OP, tableNameData, attrNames, rmsi);
    assert(rc == 0);
    if(rmsi.getNextTuple(rid, data) != RM_EOF){
        RC res = deleteTuple(SYSIDXTABLE, rid, true);
        assert(res == 0 && "delete tuple failed");
    }
    free(data);
    free(tableNameData);
    rmsi.close();
}

// Extra credit work
RC RelationManager::dropAttribute(const std::string &tableName, const std::string &attributeName) {
    return -1;
}

// Extra credit work
RC RelationManager::addAttribute(const std::string &tableName, const Attribute &attr) {
    return -1;
}

void RelationManager::printSysTable(const std::string &tableName) {
    if (!isSysTable(tableName)) return;
    FileHandle fileHandle;
    void *pageData = malloc(PAGE_SIZE);
    std::string fileName;
    TableID tableId;
    getTableInfo(tableName, tableId, fileName);
    RecordBasedFileManager::instance().openFile(fileName, fileHandle);
    for (PageNum pageNum = 0; pageNum < fileHandle.getNumberOfPages(); pageNum++) {
        fileHandle.readPage(pageNum, pageData);
        DataPage page(pageData);
        for (SlotNumber slot = 0; slot < page.getSlotNumber(); slot++) {
            RID tmp;
            if (page.checkRecordExist(slot, tmp) != 0) {
                std::cout << "DELETED SLOT" << std::endl;
                continue;
            }
            void *recordData = page.readRecord(slot);
            AttrLength attrLength;
            Record r(recordData);
            void *attrData = r.getFieldValue(0, attrLength);
            std::cout << *(TableID *) (attrData) << " ";
            free(attrData);
            attrData = r.getFieldValue(1, attrLength);

            std::string table = Record::getString(attrData, attrLength);
            std::cout << table << std::endl;
            free(attrData);
            free(recordData);
        }
    }
    free(pageData);
}

void RM_ScanIterator::setUp(const std::string &tableFileName, const std::string &conditionAttribute, const CompOp compOp,
                            const void *value, const std::vector<std::string> &attributeNames,
                            const std::vector<Attribute>& descriptor) {
    RC rc = RecordBasedFileManager::instance().openFile(tableFileName, this->fileHandle);
    assert(rc == 0 && "open file fail");
    RecordBasedFileManager::instance().scan(
            this->fileHandle, descriptor, conditionAttribute,
            compOp, value, attributeNames, this->rbfm_ScanIterator);
}

RC RM_ScanIterator::getNextTuple(RID &rid, void *data) {
    RC rc = rbfm_ScanIterator.getNextRecord(rid, data);
    if (rc == RBFM_EOF) return RM_EOF;
    else return rc;
}

RC RM_ScanIterator::close() {
    rbfm_ScanIterator.close();
    return RecordBasedFileManager::instance().closeFile(fileHandle);
}

// QE IX related
RC RelationManager::createIndex(const std::string &tableName, const std::string &attributeName) {
    RC rc;
    std::vector<Attribute> attrs;
    Attribute attribute;
    rc = getAttributes(tableName, attrs);
    if (rc != 0) return 1;
    int i;
    for (i = 0; i < attrs.size(); i++) {
        if (attrs[i].name == attributeName) {
            attribute = attrs[i];
            break;
        }
    }
    if (i == attrs.size()) {
        return 2;
    }
    IXFileHandle ixFileHandle;
    std::string fileName = tableName + "." + attributeName;
    rc = IndexManager::instance().createFile(fileName);
    if (rc != 0) return 3;
    rc = IndexManager::instance().openFile(fileName, ixFileHandle);
    if (rc != 0) return 4;
    AttrLength length;
    RM_ScanIterator rmsi;
    RID rid;
    void* data = malloc(TUPLE_TMP_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    void* key;
    std::vector<std::string> attrNames;
    attrNames.push_back(attributeName);
    rc = scan(tableName, "", NO_OP, NULL, attrNames, rmsi);
    assert(rc == 0);
    while(rmsi.getNextTuple(rid, data) != RM_EOF){
        length = 0;
        if (attribute.type == TypeVarChar) {
            memcpy(&length, (char *)data + 1, sizeof(int));
        }
        length += sizeof(int);
        key = malloc(length);
        memcpy((char *)key, (char *)data + 1, length);
        IndexManager::instance().insertEntry(ixFileHandle, attribute, key, rid);
        free(key);
    }

    free(data);
    rmsi.close();
    rc = IndexManager::instance().closeFile(ixFileHandle);
    if (rc != 0) return 5;
    addIndexInfo(fileName);
    return rc;
}

RC RelationManager::destroyIndex(const std::string &tableName, const std::string &attributeName) {
    RC rc;
    std::vector<Attribute> attrs;
    rc = getAttributes(tableName, attrs);
    if (rc != 0) return 1;
    int i;
    for (i = 0; i < attrs.size(); i++) {
        if (attrs[i].name == attributeName) {
            break;
        }
    }
    if (i == attrs.size()) {
        return 2;
    }

    std::string fileName = tableName + "." + attributeName;
    rc = IndexManager::instance().destroyFile(fileName);
    if (rc != 0) return 3;
    deleteIndexInfo(fileName);
    return rc;
}

RC RelationManager::indexScan(const std::string &tableName,
                              const std::string &attributeName,
                              const void *lowKey,
                              const void *highKey,
                              bool lowKeyInclusive,
                              bool highKeyInclusive,
                              RM_IndexScanIterator &rm_IndexScanIterator) {
    RC rc;
    std::string fileName;
    std::vector<Attribute> attrs;
    rc = getAttributes(tableName, attrs);
    if (rc != 0) return 1;
    int i;
    for (i = 0; i < attrs.size(); i++) {
        if (attributeName == attrs[i].name) {
            break;
        }
    }
    if (i == attrs.size()) return 2;

    rc = getIndexFileName(tableName, attrs[i], fileName);
    if (rc != 0) return 3;

    rm_IndexScanIterator.setUp(fileName, attrs[i], lowKey, highKey, lowKeyInclusive, highKeyInclusive);
    return 0;
}

void RM_IndexScanIterator::setUp(const std::string &fileName, const Attribute &attribute, const void *lowKey, const void *highKey, bool lowKeyInclusive, bool highKeyInclusive) {
    RC rc = IndexManager::instance().openFile(fileName, this->ixFileHandle);
    assert(rc == 0 && "open file should not fail");
    IndexManager::instance().scan(this->ixFileHandle, attribute, lowKey, highKey, lowKeyInclusive, highKeyInclusive, this->ixScanIterator);
}

RC RM_IndexScanIterator::getNextEntry(RID &rid, void *key) {
    RC rc = ixScanIterator.getNextEntry(rid, key);
    if (rc == IX_EOF) {
        return IX_EOF;
    } else {
        return rc;
    }
}
