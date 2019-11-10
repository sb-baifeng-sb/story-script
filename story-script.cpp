//
//  script.cpp
//  HelloTuiCpp
//
//  Created by baifeng on 2018/4/10.
//

#include <stdlib.h>
#include <math.h>
#include "story-script.hpp"

namespace story {

static std::vector<std::string> split(std::string const& s, std::string const& delim) {
    std::vector<std::string> elems;
    size_t pos = 0;
    size_t len = s.length();
    size_t delim_len = delim.length();
    if (delim_len == 0) return elems;
    while (pos < len)
    {
        int find_pos = s.find(delim, pos);
        if (find_pos < 0)
        {
            elems.push_back(s.substr(pos, len - pos));
            break;
        }
        elems.push_back(s.substr(pos, find_pos - pos));
        pos = find_pos + delim_len;
    }
    return elems;
}

static void removeInvalidSymbol(std::string& s) {
	while (true) {
		if (s.size() == 0) {
			return;
		}
        if (s[0] == ' ' || s[0] == '\t') {
            s = s.substr(1);
            continue;
        }
        if (s.back() == '\r' || s.back() == '\t') {
            s = s.substr(0, s.size()-1);
            continue;
        }
        break;
    }
}

//=====================================================================

ScriptData::ScriptData():mData(NULL),mDataSize(0),mLineSize(0) {
    
}

ScriptData::~ScriptData() {
    clear();
}

void ScriptData::clear() {
    if (this->mData != NULL) {
        free(this->mData);
        this->mData = NULL;
    }
    this->mDataSize = 0;
    this->mLineSize = 0;
}

void ScriptData::load(unsigned char* buffer, long bufSize) {
    
    std::string s((char*)buffer, bufSize);
    //分割换行符
    auto list = split(s, "\n");
    //printf("line: %d\n", (int)list.size());
    for (int i=0; i < (int)list.size(); i++) {
        std::string& line = list[i];
        // 过滤无效字符
        removeInvalidSymbol(line);
    }
    
    std::vector<std::string> array;
    array.reserve(list.size());
    for (int i=0; i < (int)list.size(); i++) {
        if (list[i] == "block") {
            std::string line;
            do {
                ++i;
                assert(i < (int) list.size() || "ScriptData::load /block no match.");
                if (list[i] == "/block") {
                    break;
                }
                line += list[i] + "\n";
            } while (true);
            if (line.length() != 0) {
                array.push_back(line);
            }
            continue;
        }
        if (list[i].length() != 0) {
            array.push_back(list[i]);
        }
    }
    
    int realSize = 0;
    std::vector<int> indexArray;
    indexArray.reserve(array.size());
    
    for (int i=0; i < array.size(); i++) {
        std::string const& line = array[i];
        int size = (int)line.size();
        if (size != 0) {
            indexArray.push_back(i);
            realSize += size;
        }
    }
    clear();
    
    this->mDataSize = realSize+sizeof(int)*indexArray.size();
    this->mData = (unsigned char*)malloc(this->mDataSize);
    this->mLineSize = indexArray.size();
    memset(this->mData, 0, this->mDataSize);
    
    int offset = 0;
    for (int i=0; i < indexArray.size(); i++) {
        int index = (int)indexArray[i];
        std::string const& line = array[index];
        int size = (int)line.size();
        unsigned char* curr = this->mData + offset;
        memcpy(curr, &size, sizeof(int));
        curr += sizeof(int);
        memcpy(curr, line.data(), line.size());
        offset += sizeof(int) + size;
    }
}

void ScriptData::load(unsigned char* buffer, long bufSize, std::string const& filename) {
    this->load(buffer, bufSize);
    this->mFileName = filename;
}

void ScriptData::travel(TraveralFunc func) const {
    int offset = 0;
    for (int i=0; i < this->mLineSize; i++) {
        unsigned char* buffer = this->mData + offset;
        int bufSize = 0;
        memcpy(&bufSize, buffer, sizeof(int));
        func(i, buffer+sizeof(int), bufSize);
        offset += bufSize + sizeof(int);
    }
}

long ScriptData::getDataSize() const {
    return this->mDataSize;
}

std::string const& ScriptData::getFileName() const {
    return this->mFileName;
}

//=====================================================================

Script::Script():mIndex(0), mCurTime(0), mEndTime(0) {
    mScript.reserve(1024);
}

Script::~Script() {
    clear();
}

void Script::load(ScriptData const& data) {
    clear();
    data.travel(std::bind(
        &Script::onAdd, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    );
    this->mName = data.getFileName();
}

void Script::back(int step) {
    mIndex -= fabs((float)step);
}

void Script::seek(std::string const& tag) {
    int linenum = getTagValue(tag);
    if (linenum >= 0) {
        this->seek(linenum);
    }
}

void Script::seek(int index) {
    if (index >= 0) {
        mIndex = index;
    }
}

void Script::step(ScriptStepFunc func) {
    if (isEnd()) {
        return;
    }
    std::string const& line = mScript[mIndex];
    func(line, mIndex++, mName);
}

bool Script::isEnd() {
    return mIndex >= mScript.size();
}

void Script::update(float dt) {
    if (isPause()) {
        this->mCurTime += dt * 1000;
        if (this->mCurTime >= this->mEndTime) {
            this->mCurTime = 0;
            this->mEndTime = 0;
        }
    }
}

void Script::pause(float seconds) {
    this->mCurTime = 0;
    this->mEndTime = seconds * 1000;
    if (this->mEndTime < 0) {
        this->mEndTime = 0x0fffffff;
    }
}

bool Script::isPause() const {
    return this->mCurTime != this->mEndTime;
}

void Script::setPauseTime(int curTime, int endTime) {
    this->mCurTime = curTime;
    this->mEndTime = endTime;
}

int Script::getIndex() const {
    return mIndex;
}
int Script::getCurTime() const {
    return mCurTime;
}
int Script::getEndTime() const {
    return mEndTime;
}
std::string const& Script::getName() const {
    return mName;
}

void Script::clear() {
    mScript.clear();
    mTags.clear();
    mIndex = 0;
}

int Script::getTagValue(std::string const& tag) const {
    TagMap::const_iterator iter = mTags.find(tag);
    if (mTags.end() == iter) {
        return -1;
    }
    return iter->second;
}

void Script::onAdd(int index, unsigned char* curr_buffer, long buf_size) {
    std::string line((char*)curr_buffer, buf_size);
    mScript.push_back(line);
    if (line[0] == '*') {
        mTags[line] = index;
    }
}

}
