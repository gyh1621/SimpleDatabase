#include "rm.h"

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
    std::vector<Attribute> tablesAttrs;
    std::vector<Attribute> columnsAttrs;
    getSysTableAttributes(tablesAttrs);
    getSysColTableAttributes(columnsAttrs);
    addMetaInfo(SYSTABLE, tablesAttrs);
    addMetaInfo(SYSCOLTABLE, columnsAttrs);
    return rc;
}

RC RelationManager::deleteCatalog() {
    RC rc;
    rc = RecordBasedFileManager::instance().destroyFile(SYSTABLE);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().destroyFile(SYSCOLTABLE);
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
        if(rc != 0) return rc;
        deleteMetaInfo(tableName);
    }else{
        return -1;
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
    if (rc != 0) return rc;

    void *data = malloc(TUPLE_TMP_SIZE);
    RID rid;

    AttrLength attrLength;
    while (rmsi.getNextTuple(rid, data) != RM_EOF) {
        Record r(descriptor, data);
        Attribute attr;
        void *data = r.getFieldValue(1, attrLength);  // column-name
        if (data == nullptr) throw std::bad_alloc();
        attr.name = Record::getString(data, attrLength);
        free(data);
        data = r.getFieldValue(2, attrLength);  // column-type
        attr.type = *((AttrType *) data);
        free(data);
        data = r.getFieldValue(3, attrLength);  // column-length
        attr.length = *((AttrLength *) data);
        free(data);
        attrs.push_back(attr);
    }

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
    RC rc;
    rc = getTableInfo(tableName, id, fileName);
    if(rc != 0) return -2;
    rc = RecordBasedFileManager::instance().openFile(fileName, fileHandle);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().updateRecord(fileHandle, attr, data, rid);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().closeFile(fileHandle);
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
    if (rc != 0) return rc;
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
    return rc;
}

RC RelationManager::deleteTuple(const std::string &tableName, const RID &rid, bool sys) {
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
    rc = RecordBasedFileManager::instance().deleteRecord(fileHandle, attr, rid);
    if(rc != 0) return rc;
    rc = RecordBasedFileManager::instance().closeFile(fileHandle);
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

void RelationManager::addMetaInfo(const std::string &tableName, const std::vector<Attribute> &descriptor) {
    TableID id = ++tableNumber;
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
    PageNum pageNum = fileHandle.dataPageNum;
    void* page = malloc(PAGE_SIZE);
    if (page == nullptr) throw std::bad_alloc();
    for(int i = 0; i < pageNum; i++){
        fileHandle.readPage(i, page);
        DataPage p(page);
        count += p.getRecordNumber();
    }
    RecordBasedFileManager::instance().closeFile(fileHandle);
    free(page);
    return count;
}

void RelationManager::addTablesInfo(const std::string &tableName, TableID id) {
    void* data = malloc(TUPLE_TMP_SIZE);
    if (data == nullptr) throw std::bad_alloc();
    auto nullPointerSize = ceil(3 / 8.0);
    auto* nullPointer = (unsigned char*)data;
    for(int i = 0; i < nullPointerSize; i++) nullPointer[i] = 0;

    // null pointer
    memcpy((char *) data, nullPointer, nullPointerSize);
    int dataOffset = nullPointerSize;
    // table-id
    memcpy((char *)data + dataOffset, &id, sizeof(TableID));
    // table-name
    dataOffset += sizeof(TableID);
    int length = tableName.size();
    memcpy((char *)data + dataOffset, &length, 4);
    dataOffset += 4;
    memcpy((char *) data + dataOffset, tableName.c_str(), length);
    dataOffset += length;
    //file-name;
    memcpy((char *)data + dataOffset, &length, sizeof(int));
    dataOffset += 4;
    memcpy((char *) data + dataOffset, tableName.c_str(), length);

    RID rid;
    insertTuple(SYSTABLE, data, rid, true);
    free(data);
}

void RelationManager::addColumnsInfo(const std::string &tableName, TableID id, const std::vector<Attribute> &descriptor) {
    std::vector<Attribute> colAttr;
    getSysColTableAttributes(colAttr);

    auto nullPointerSize = ceil(5 / 8.0);
    void* data;
    int dataOffset;
    for(int i = 0; i < descriptor.size(); i++){
        data = malloc(200);
        auto* nullPointer = (unsigned char*)data;
        for(int j = 0; j < nullPointerSize; j++) nullPointer[j] = 0;
        dataOffset = nullPointerSize;
        // id
        memcpy((char *)data + dataOffset, &id, sizeof(int));
        dataOffset += 4;
        // column-name
        int length = descriptor[i].name.size();
        memcpy((char *)data + dataOffset, &length, sizeof(int));
        dataOffset += 4;
        memcpy((char *) data + dataOffset, descriptor[i].name.c_str(), length);
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
        insertTuple(SYSCOLTABLE, data, rid, true);
        free(data);
    }
}

RC RelationManager::deleteMetaInfo(const std::string &tableName) {
    TableID id;
    std::string fileName;
    getTableInfo(tableName, id, fileName);

    RM_ScanIterator rmsi;
    RID rid;
    void* data = malloc(TUPLE_TMP_SIZE);
    if(data == nullptr) throw std::bad_alloc();

    // delete record in sys table
    std::vector<Attribute> descriptor;
    getSysTableAttributes(descriptor);
    std::vector<std::string> attrNames;
    getDescriptorString(descriptor, attrNames);
    RC rc;
    void *tableNameData = createVarcharData(tableName);
    rc = scan(SYSTABLE, "table-name", EQ_OP, tableNameData, attrNames, rmsi);
    if(rc != 0) {
        free(tableNameData);
        free(data);
        return rc;
    }
    if(rmsi.getNextTuple(rid, data) != RM_EOF){
        RC res = deleteTuple(SYSTABLE, rid, true);
        assert(res == 0 && "delete tuple failed");
    }
    descriptor.clear();
    attrNames.clear();
    free(tableNameData);

    // delete record in sys column
    getSysColTableAttributes(descriptor);
    getDescriptorString(descriptor, attrNames);
    rc = scan(SYSCOLTABLE, "table-id", EQ_OP, &id, attrNames, rmsi);
    if(rc != 0) {
        free(data);
        return rc;
    }
    while(rmsi.getNextTuple(rid, data) != RM_EOF){
        RC res = deleteTuple(SYSCOLTABLE, rid);
        assert(res == 0 && "delete tuple failed");
    }
    free(data);

    return 0;
}

void RelationManager::getDescriptorString(const std::vector<Attribute> &descriptor, std::vector<std::string> &attrNames) {
    for(int i = 0; i < descriptor.size(); i++){
        attrNames.push_back(descriptor[i].name);
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
    RM_ScanIterator rmsi;
    RID rid;
    void* data = malloc(200);
    std::vector<Attribute> descriptor;
    getSysTableAttributes(descriptor);
    std::vector<std::string> attrNames;
    getDescriptorString(descriptor, attrNames);
    RC rc;
    void *tableNameData = createVarcharData(tableName);
    rc = scan(SYSTABLE, "table-name", EQ_OP, tableNameData, attrNames, rmsi);
    if(rc != 0) return rc;
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
        memcpy(varchar, (char *)data + offset, length);
        std::string s(varchar, length);
        delete[](varchar);
        //get fileHandle
        fileName = s;
    }else{
        return -2;
    }
    free(data);
    return 0;
}

bool RelationManager::isSysTable(const std::string &tableName) {
    return tableName == SYSTABLE || tableName == SYSCOLTABLE;
}

void* RelationManager::createVarcharData(const std::string &str) {
    const char *chars = str.c_str();
    void *data = malloc(4 + str.length());
    if (data == nullptr) throw std::bad_alloc();
    unsigned length = str.length();
    memcpy(data, &length, 4);
    memcpy((char *) data + 4, chars, str.length());
    return data;
}

// Extra credit work
RC RelationManager::dropAttribute(const std::string &tableName, const std::string &attributeName) {
    return -1;
}

// Extra credit work
RC RelationManager::addAttribute(const std::string &tableName, const Attribute &attr) {
    return -1;
}


void RM_ScanIterator::setUp(const std::string &tableFileName, const std::string &conditionAttribute, const CompOp compOp,
                            const void *value, const std::vector<std::string> &attributeNames,
                            const std::vector<Attribute> descriptor) {
    RecordBasedFileManager::instance().openFile(tableFileName, this->fileHandle);
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
