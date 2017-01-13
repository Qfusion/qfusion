#include "ai_weight_config.h"

template <typename T>
inline void AiBaseWeightConfigVarGroup::LinkItem(T *item, T **linkedItemsHead, T ***hashBins, unsigned *numItems)
{
    item->nextSibling = (*linkedItemsHead);
    (*linkedItemsHead) = item;

    (*numItems)++;
    if ((*numItems) < MIN_HASH_ITEMS)
        return;

    if ((*numItems) > MIN_HASH_ITEMS)
    {
        AddItemToHashBin(item, hashBins);
        return;
    }

    auto memSize = NUM_HASH_BINS * sizeof(T *);
    (*hashBins) = (T **)G_Malloc(memSize);
    memset((*hashBins), 0, memSize);

    for (auto linkedItem = *linkedItemsHead; linkedItem; linkedItem = linkedItem->nextSibling)
        AddItemToHashBin(linkedItem, hashBins);
}

template <typename T>
inline void AiBaseWeightConfigVarGroup::AddItemToHashBin(T *item, T ***hashBins)
{
    unsigned binIndex = item->nameHash % NUM_HASH_BINS;
    item->nextInHashBin = (*hashBins)[binIndex];
    (*hashBins)[binIndex] = item;
}

void AiBaseWeightConfigVarGroup::LinkGroup(AiBaseWeightConfigVarGroup *childGroup)
{
    LinkItem(childGroup, &childGroupsHead, &groupsHashBins, &numChildGroups);
}

void AiBaseWeightConfigVarGroup::LinkVar(AiBaseWeightConfigVar *childVar)
{
    LinkItem(childVar, &childVarsHead, &varsHashBins, &numChildVars);
}

AiBaseWeightConfigVarGroup::~AiBaseWeightConfigVarGroup()
{
    // These loops are written to avoid an access to free-ed memory even if its harmless but can trigger an analyzer

    auto *scriptVar = allocatedVarsHead;
    while (scriptVar)
    {
        auto *nextVar = scriptVar->nextAllocated;
        scriptVar->~AiScriptWeightConfigVar();
        G_Free(scriptVar);
        scriptVar = nextVar;
    }

    auto *scriptGroup = allocatedGroupsHead;
    while (scriptGroup)
    {
        auto *nextGroup = scriptGroup->nextAllocated;
        scriptGroup->~AiScriptWeightConfigVarGroup();
        G_Free(scriptGroup);
        scriptGroup = nextGroup;
    }

    // If hash bins were allocated
    if (varsHashBins != &childVarsHead)
        G_Free(varsHashBins);

    if (groupsHashBins != &childGroupsHead)
        G_Free(groupsHashBins);
}

template <typename T>
inline void AiBaseWeightConfigVarGroup::AddScriptItem(const char *name_, void *scriptObject, T **allocatedItemsHead)
{
    T *scriptItem = new(G_Malloc(sizeof(T)))T(this, name_, scriptObject);
    scriptItem->nextAllocated = (*allocatedItemsHead);
    (*allocatedItemsHead) = scriptItem;
}

void AiBaseWeightConfigVarGroup::AddScriptGroup(const char *name_, void *scriptObject)
{
    AddScriptItem<AiScriptWeightConfigVarGroup>(name_, scriptObject, &allocatedGroupsHead);
}

void AiBaseWeightConfigVarGroup::AddScriptVar(const char *name_, void *scriptObject_)
{
    AddScriptItem<AiScriptWeightConfigVar>(name_, scriptObject_, &allocatedVarsHead);
}

template <typename T>
inline T *AiBaseWeightConfigVarGroup::GetItemByName(const char *name_, unsigned nameHash_,
                                                    T *childItemsHead, T **hashBins, unsigned numItems)
{
    // We do not check name length to cut off string comparison.
    // However situations when name hash match and length does not should be extremely rare,
    // so adding an extra nameLength argument to this call would be silly for only such rare case.

    if (numItems < MIN_HASH_ITEMS)
    {
        // Do not force name hash computation when there are few items, its should be faster to compare strings as is
        if (!nameHash_)
        {
            for (auto *childItem = childItemsHead; childItem; childItem = childItem->nextSibling)
            {
                if (!Q_stricmp(childItem->name, name_))
                    return childItem;
            }
            return nullptr;
        }

        for (auto *childItem = childItemsHead; childItem; childItem = childItem->nextSibling)
        {
            if (childItem->nameHash != nameHash_)
                continue;

            if (!Q_stricmp(childItem->name, name_))
                return childItem;
        }

        return nullptr;
    }

    unsigned dummy;
    if (!nameHash_)
        GetHashAndLength(name_, &nameHash_, &dummy);

    unsigned binIndex = nameHash_ % NUM_HASH_BINS;
    for (auto item = hashBins[binIndex]; item; item = item->nextInHashBin)
    {
        if (item->nameHash != nameHash_)
            continue;

        if (!Q_stricmp(item->name, name_))
            return item;
    }

    return nullptr;
}

AiBaseWeightConfigVarGroup *AiBaseWeightConfigVarGroup::GetGroupByName(const char *name_, unsigned int nameHash_)
{
    return GetItemByName(name_, nameHash_, childGroupsHead, groupsHashBins, numChildGroups);
}

AiBaseWeightConfigVar *AiBaseWeightConfigVarGroup::GetVarByName(const char *name_, unsigned nameHash_)
{
    return GetItemByName(name_, nameHash_, childVarsHead, varsHashBins, numChildVars);
}

template <typename T>
inline T *AiBaseWeightConfigVarGroup::GetItemByPath(const char *path,
                                                    T *(AiBaseWeightConfigVarGroup::*getByNameMethod)(const char *, unsigned))
{
    const char *nextSeparator = strchr(path, '/');
    if (!nextSeparator)
        return (this->*getByNameMethod)(path, 0);

    auto pathHeadLength = nextSeparator - path;
    const char *restOfThePath = nextSeparator;
    // Allow duplicated separators
    while (*restOfThePath == '/')
        restOfThePath++;

    // Do not force path hash computation
    unsigned pathHeadHash = 0;
    for (auto *childGroup = childGroupsHead; childGroup; childGroup = childGroup->nextSibling)
    {
        if (childGroup->NameLength() != pathHeadLength)
            continue;
        if (pathHeadHash == 0)
            pathHeadHash = GetHashForLength(path, (unsigned)pathHeadLength);
        if (childGroup->NameHash() != pathHeadHash)
            continue;
        if (!Q_strnicmp(childGroup->name, path, pathHeadLength))
            return childGroup->GetItemByPath(restOfThePath, getByNameMethod);
    }
    return nullptr;
}

AiBaseWeightConfigVarGroup *AiBaseWeightConfigVarGroup::GetGroupByPath(const char *path)
{
    return GetItemByPath<AiBaseWeightConfigVarGroup>(path, &AiBaseWeightConfigVarGroup::GetGroupByName);
}

AiBaseWeightConfigVar *AiBaseWeightConfigVarGroup::GetVarByPath(const char *path)
{
    return GetItemByPath<AiBaseWeightConfigVar>(path, &AiBaseWeightConfigVarGroup::GetVarByName);
}

void AiBaseWeightConfigVarGroup::ResetToDefaultValues()
{
    for (auto *childGroup = childGroupsHead; childGroup; childGroup = childGroup->nextSibling)
        childGroup->ResetToDefaultValues();

    for (auto *childVar = childVarsHead; childVar; childVar = childVar->nextSibling)
        childVar->ResetToDefaultValues();
}

void AiBaseWeightConfigVarGroup::CheckTouched(const char *parentName)
{
    if (!isTouched)
    {
        if (parentName)
            G_Printf(S_COLOR_YELLOW "WARNING: Group %s in group %s has not been touched\n", name, parentName);
        else
            G_Printf(S_COLOR_YELLOW "WARNING: Group %s has not been touched\n", name);
    }
    isTouched = false;

    for (auto *childGroup = childGroupsHead; childGroup; childGroup = childGroup->nextSibling)
        childGroup->CheckTouched(this->name);

    for (auto *childVar = childVarsHead; childVar; childVar = childVar->nextSibling)
        childVar->CheckTouched(this->name);
}

void AiBaseWeightConfigVarGroup::Touch(const char *parentName)
{
    if (isTouched)
    {
        if (parentName)
            G_Printf(S_COLOR_YELLOW "WARNING: Group %s in group %s has been already touched\n", name, parentName);
        else
            G_Printf(S_COLOR_YELLOW "WARNING: Group %s has been already touched\n", name);
    }
    isTouched = true;
}

void AiBaseWeightConfigVarGroup::CopyValues(const AiBaseWeightConfigVarGroup &that)
{
    auto groupsIterator = ZipItemChains(this->childGroupsHead, that.childGroupsHead, "Copying groups");
    for (; groupsIterator.HasNext(); groupsIterator.Next())
        groupsIterator.First()->CopyValues(*groupsIterator.Second());

    auto varsIterator = ZipItemChains(this->childVarsHead, that.childVarsHead, "Copying vars");
    for (; varsIterator.HasNext(); varsIterator.Next())
    {
        float value, minValue, maxValue, defaultValue;
        varsIterator.Second()->GetValueProps(&value, &minValue, &maxValue, &defaultValue);
        varsIterator.First()->SetValue(value);
    }
}

bool AiBaseWeightConfigVarGroup::operator==(const AiBaseWeightConfigVarGroup &that) const
{
    // WARNING! Compare values, not pointers! That's why it's shown explicitly.
    auto groupsIterator = ZipItemChains(this->childGroupsHead, that.childGroupsHead, "Comparing groups");
    for (; groupsIterator.HasNext(); groupsIterator.Next())
        if (groupsIterator.First()->operator!=(*groupsIterator.Second()))
            return false;

    auto varsIterator = ZipItemChains(this->childVarsHead, that.childVarsHead, "Comparing vars");
    for (; varsIterator.HasNext(); varsIterator.Next())
        if (varsIterator.First()->operator!=(*varsIterator.Second()))
            return false;

    return true;
}

bool AiBaseWeightConfigVarGroup::Parse(const char *data, const char **restOfTheData)
{
    int status = 0;
    do
    {
        status = ParseNextEntry(data, restOfTheData);
        data = *restOfTheData;
    } while (status > 0);

    // This means no items are left and no errors occured
    return (status == 0);
}

// We have to roll this on our own to be locale-independent
static bool BasicStrtof(const char *str, float *result)
{
    double integerPart = 1.0f;
    // TODO: Skip leading 0's?
    if (*str < '0' || *str > '9')
    {
        if (*str == '-')
            integerPart = -integerPart;
        else if (*str != '+')
            return false;
    }

    integerPart *= *str - '0';
    str++;
    while (*str >= '0' && *str <= '9')
    {
        integerPart *= 10;
        integerPart += *str - '0';
        str++;
    }

    if (*str != '.')
    {
        if (!*str)
        {
            *result = (float)integerPart;
            return true;
        }
        return false;
    }
    str++;

    double fractionalPart = 0.0;
    double digitScale = 0.1;
    while (*str >= '0' && *str <= '9')
    {
        fractionalPart += digitScale * (*str - '0');
        digitScale *= 0.1;
        str++;
    }

    // Forbid any trailing characters
    if (*str)
        return false;

    // We split integer and fractional parts to avoid fp computation errors when operands exponent significantly differ
    *result = (float)(integerPart + fractionalPart);

    return true;
}

int AiBaseWeightConfigVarGroup::ParseNextEntry(const char *data, const char **nextData)
{
    char firstToken[MAX_TOKEN_CHARS];
    COM_Parse_r(firstToken, MAX_TOKEN_CHARS, &data);
    if (!Q_stricmp(firstToken, "}"))
    {
        *nextData = data;
        return 0;
    }

    const char *token = COM_Parse(&data);
    constexpr const char *function = "AiBaseWeightConfigVarGroup::ReadNextEntry()";

    *nextData = data;
    // If a group start can be matched
    if (!Q_stricmp(token, "{"))
    {
        // If such group is registered, let it parse itself
        if (AiBaseWeightConfigVarGroup *group = GetGroupByName(firstToken))
        {
            if (!group->Parse(data, nextData))
                return -1;
            else
                group->Touch(this->Name());
        }
        else
        {
            G_Printf(S_COLOR_RED "%s: Unknown group name `%s`\n", function, token);
            return -1;
        }
        return 1;
    }

    if (AiBaseWeightConfigVar *var = GetVarByName(firstToken))
    {
        float value;
        if (!BasicStrtof(token, &value))
        {
            G_Printf(S_COLOR_RED "%s: Expected a floating-point numeric value for the `%s` var\n", function, firstToken);
            return -1;
        }
        var->SetValue(value);
        var->Touch(this->Name());
        return 1;
    }

    G_Printf(S_COLOR_RED "%s: Illegal group entry in the `%s` group\n", function, this->name);
    return -1;
}

#define CHECK_WRITE(message)                       \
do                                                 \
{                                                  \
    if (trap_FS_Print(fileHandle, (message)) <= 0) \
        return false;                              \
}                                                  \
while (0)

#define WRITE_INDENTS(depth)                       \
do                                                 \
{                                                  \
    for (int i = 0; i < (depth); ++i)              \
        if (trap_FS_Print(fileHandle, "\t") <= 0)  \
            return false;                          \
}                                                  \
while (0)

bool AiBaseWeightConfigVarGroup::Write(int fileHandle, int depth) const
{
    WRITE_INDENTS(depth);
    CHECK_WRITE(this->name);
    CHECK_WRITE("\r\n");

    WRITE_INDENTS(depth);
    CHECK_WRITE("{\r\n");

    float value, minValue, maxValue, defaultValue;
    for (auto *childVar = childVarsHead; childVar; childVar = childVar->nextSibling)
    {
        childVar->GetValueProps(&value, &minValue, &maxValue, &defaultValue);
        WRITE_INDENTS(depth + 1);
        CHECK_WRITE(va("%s %.6f\r\n", childVar->name, value));
    }

    for (auto *childGroup = childGroupsHead; childGroup; childGroup = childGroup->nextSibling)
        childGroup->Write(fileHandle, depth + 1);

    WRITE_INDENTS(depth);
    CHECK_WRITE("}\r\n");
    return true;
}

inline const char *AiWeightConfig::SkipRootInPath(const char *path) const
{
    while (*path == '/')
        path++;

    const char *nextSeparator = strchr(path, '/');
    if (!nextSeparator)
        return nullptr;

    auto len = nextSeparator - path;
    if (Q_strnicmp(path, this->name, len))
        return nullptr;

    while (*nextSeparator == '/')
        nextSeparator++;

    return nextSeparator;
}

AiBaseWeightConfigVarGroup *AiWeightConfig::GetGroupByPath(const char *path)
{
    if (const char *pathAfterRoot = SkipRootInPath(path))
        return AiBaseWeightConfigVarGroup::GetGroupByPath(pathAfterRoot);
    return nullptr;
}

AiBaseWeightConfigVar *AiWeightConfig::GetVarByPath(const char *path)
{
    if (const char *pathAfterRoot = SkipRootInPath(path))
        return AiBaseWeightConfigVarGroup::GetVarByPath(pathAfterRoot);
    return nullptr;
}

bool AiWeightConfig::LoadFromData(const char *data)
{
    const char *token = COM_Parse(&data);
    if (Q_stricmp(token, this->name))
    {
        G_Printf(S_COLOR_RED "AiWeightConfig::Load(): Expected %s root weights group name, got %s\n", this->name, token);
        return false;
    }

    token = COM_Parse(&data);
    if (Q_stricmp(token, "{"))
    {
        G_Printf(S_COLOR_RED "AiWeightConfig::Load(): Missing root group {\n");
        return false;
    }

    const char *restOfTheData;
    if (!AiBaseWeightConfigVarGroup::Parse(data, &restOfTheData))
        return false;

    data = restOfTheData;
    token = COM_Parse(&data);
    if (strlen(token) > 0)
    {
        G_Printf(S_COLOR_RED "AIWeightConfig::Load(): Unexpected extra token `%s` at the end of file\n", token);
        return false;
    }

    // Touch the root (AiBaseWeightConfigVarGroup::Parse() does not perform it for the parsed group itself)
    this->Touch();
    // Check whether all subgroups and vars have been touched
    CheckTouched();
    return true;
}

bool AiWeightConfig::Load(const char *filename)
{
    int fileHandle;
    int fileSize = trap_FS_FOpenFile(filename, &fileHandle, FS_READ);
    if (fileSize <= 0)
    {
        G_Printf(S_COLOR_RED "AiWeightConfig::Load(): Can't open file `%s`\n", filename);
        return false;
    }

    // Ensure that the buffer is zero-terminated
    char *buffer = (char *)G_LevelMalloc((unsigned)fileSize + 1);
    buffer[fileSize] = 0;

    int bytesRead = trap_FS_Read(buffer, (unsigned)fileSize, fileHandle);
    if (bytesRead != fileSize)
    {
        G_LevelFree(buffer);
        trap_FS_FCloseFile(fileHandle);
        const char *format = S_COLOR_RED "AIWeightConfig()::Load(): only %d/%d bytes of file %s can be read\n";
        G_Printf(format, bytesRead, fileSize, filename);
        return false;
    }

    bool result = LoadFromData(buffer);
    G_LevelFree(buffer);
    trap_FS_FCloseFile(fileHandle);
    return result;
}

bool AiWeightConfig::Save(const char *filename)
{
    int fileHandle;
    if (trap_FS_FOpenFile(filename, &fileHandle, FS_WRITE) < 0)
        return false;

    bool result = Write(fileHandle, 0);
    trap_FS_FCloseFile(fileHandle);
    return result;
}


