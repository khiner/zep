#include <cassert>
#include <regex>

#include "zep/keymap.h"
#include "zep/mode.h"

#include "zep/mcommon/logger.h"

namespace Zep {

// Splitting the input into groups of <> or ch
std::string NextToken(std::string::const_iterator &itrChar, std::string::const_iterator itrEnd) {
    std::ostringstream str;

    // Find a group
    if (*itrChar == '<') {
        itrChar++;
        auto itrStart = itrChar;

        // Walk the group, ensuring we consistently output (C-)(S-)foo
        while (itrChar != itrEnd && *itrChar != '>') {
            itrChar++;
        }

        // Ensure <C-S- or variants, ie. capitalize for consistency if the mapping
        // was badly supplied
        auto strGroup = std::string(itrStart, itrChar);
        string_replace_in_place(strGroup, "c-", "C-");
        string_replace_in_place(strGroup, "s-", "S-");

        // Skip to the next
        if (itrChar != itrEnd)
            itrChar++;

        str << "<" << strGroup << ">";
    } else {
        str << *itrChar++;
    }

    return str.str();
}

// Add a collection of commands to a collection of mappings
bool keymap_add(const std::vector<KeyMap *> &maps, const std::vector<std::string> &commands, const StringId &commandId, KeyMapAdd option) {
    bool ret = true;
    for (auto &map: maps) {
        for (auto &cmd: commands) {
            if (!keymap_add(*map, cmd, commandId, option))
                ret = false;
        }
    }
    return ret;
}

bool keymap_add(KeyMap &map, const std::string &strCommand, const StringId &commandId, KeyMapAdd option) {
    auto current = map.root;

    std::ostringstream str;
    auto itrChar = strCommand.begin();
    while (itrChar != strCommand.end()) {
        auto search = NextToken(itrChar, strCommand.end());

        auto itrRoot = current->children.find(search);
        if (itrRoot == current->children.end()) {
            auto node = std::make_shared<CommandNode>();
            node->token = search;
            current->children[search] = node;
            current = node;
        } else {
            current = itrRoot->second;
        }
    }

    if (current->commandId != 0 && option == KeyMapAdd::New) {
        assert(!"Adding twice?");
        return false;
    }

    current->commandId = commandId;
    return true;
}

void keymap_dump(const KeyMap &map, std::ostringstream &str) {
    std::function<void(std::shared_ptr<CommandNode>, int)> fnDump;
    fnDump = [&](std::shared_ptr<CommandNode> node, int depth) {
        for (int i = 0; i < depth; i++) {
            str << " ";
        }
        str << node->token;
        if (node->commandId != 0)
            str << " : " << node->commandId.ToString();
        str << std::endl;

        for (auto &child: node->children) {
            fnDump(child.second, depth + 2);
        }
    };
    fnDump(map.root, 0);
}

bool isDigit(const char ch) { return ch >= '0' && ch <= '9'; }

// Walk the tree of tokens, figuring out which command this is
// Input to this function:
// <C-x>fgh
// i.e. Keyboard mappings are fed in as <> strings.
void keymap_find(const KeyMap &map, const std::string &strCommand, KeyMapResult &findResult) {
    auto consumeDigits = [](std::shared_ptr<CommandNode> &node, std::string::const_iterator &itrChar, std::string::const_iterator itrEnd, std::vector<int> &result, std::ostringstream &str) {
        if (node->token == "<D>") {
            // Walk along grabbing digits
            auto itrStart = itrChar;
            while (itrChar != itrEnd && isDigit(*itrChar)) {
                itrChar++;
            }

            if (itrStart != itrChar) {
                auto token = std::string(itrStart, itrChar);
                try {
                    // Grab the data, but continue to search for the next token
                    result.push_back(std::stoi(token));
                    str << "(D:" << token << ")";
                }
                catch (std::exception &ex) {
                    ZEP_UNUSED(ex);
                    ZLOG(DBG, ex.what());
                }
                return true;
            }
        }
        return false;
    };

    auto consumeChar = [](std::shared_ptr<CommandNode> &node, std::string::const_iterator &itrChar, std::string::const_iterator itrEnd, std::vector<char> &chars, std::ostringstream &str) {
        if (node->token == "<.>") {
            // Special match groups
            if (itrChar != itrEnd) {
                chars.push_back(*itrChar);
                str << "(." << *itrChar << ")";
                itrChar++;
                return true;
            }
        }
        return false;
    };

    auto consumeRegister = [](std::shared_ptr<CommandNode> &node, std::string::const_iterator &itrChar, std::string::const_iterator itrEnd, std::vector<char> &registers, std::ostringstream &str) {
        if (node->token == "<R>") {
            // Grab register
            if (itrChar != itrEnd && *itrChar == '"') {
                itrChar++;
                if (itrChar != itrEnd) {
                    registers.push_back(*itrChar);
                    str << "(\"" << *itrChar << ")";
                    itrChar++;
                }
                return true;
            }
        }
        return false;
    };

    struct Captures {
        std::vector<int> captureNumbers;
        std::vector<char> captureChars;
        std::vector<char> captureRegisters;
    };

    std::function<bool(std::shared_ptr<CommandNode>, std::string::const_iterator, std::string::const_iterator, const Captures &captures, KeyMapResult &)> fnSearch;
    fnSearch = [&](std::shared_ptr<CommandNode> node, std::string::const_iterator itrChar, std::string::const_iterator itrEnd, const Captures &captures, KeyMapResult &result) {
        for (auto &child: node->children) {
            auto childNode = child.second;
            std::string::const_iterator itr = itrChar;

            Captures nodeCaptures;
            std::ostringstream strCaptures;

            std::string token;

            // Consume wildcards
            if (consumeDigits(childNode, itr, itrEnd, nodeCaptures.captureNumbers, strCaptures)) {
                token = childNode->token;
            } else if (consumeRegister(childNode, itr, itrEnd, nodeCaptures.captureRegisters, strCaptures)) {
                token = childNode->token;
            } else if (consumeChar(childNode, itr, itrEnd, nodeCaptures.captureChars, strCaptures)) {
                token = childNode->token;
            } else {
                // Grab full <C-> tokens
                token = string_slurp_if(itr, itrEnd, '<', '>');
                if (token.empty() && itr != itrEnd) {
                    // ... or next char
                    token = std::string(itr, itr + 1);
                    string_eat_char(itr, itrEnd);
                }
            }

            if (token.empty() && child.second->commandId == StringId() && !childNode->children.empty()) {
                result.searchPath += "(...)";
                result.needMoreChars = true;
                continue;
            }

            // We found a matching token or wildcard token at this level
            if (child.first == token) {
                // Remember what we found
                result.searchPath += strCaptures.str() + "(" + token + ")";

                // Remember if this is a valid match for something
                result.foundMapping = childNode->commandId;

                // Append our capture groups to the current hierarchy level
                nodeCaptures.captureChars.insert(nodeCaptures.captureChars.end(), captures.captureChars.begin(), captures.captureChars.end());
                nodeCaptures.captureNumbers.insert(nodeCaptures.captureNumbers.end(), captures.captureNumbers.begin(), captures.captureNumbers.end());
                nodeCaptures.captureRegisters.insert(nodeCaptures.captureRegisters.end(), captures.captureRegisters.begin(), captures.captureRegisters.end());

                // This node doesn't have a mapping, so look harder
                if (result.foundMapping == StringId()) {
                    // There are more children, and we haven't got any more characters, keep asking for more
                    if (!childNode->children.empty() && itr == itrEnd) {
                        result.needMoreChars = true;
                    } else {
                        // Walk down to the next level
                        if (fnSearch(childNode, itr, itrEnd, nodeCaptures, result)) return true;
                    }
                } else {
                    // This is the find result, note it and record the capture groups for the find
                    result.searchPath += " : " + childNode->commandId.ToString();
                    result.captureChars = nodeCaptures.captureChars;
                    result.captureNumbers = nodeCaptures.captureNumbers;
                    result.captureRegisters = nodeCaptures.captureRegisters;
                    result.needMoreChars = false;
                    return true;
                }
            }
        };

        // Searched and found nothing in this level
        return false;

    }; // fnSearch

    findResult.needMoreChars = false;

    Captures captures;
    bool found = fnSearch(map.root, strCommand.begin(), strCommand.end(), captures, findResult);
    if (!found) {
        if (findResult.needMoreChars) {
            findResult.searchPath += "(...)";
        } else {
            // Special case where the user typed a j followed by _not_ a k.
            // Return it as an insert command
            if (strCommand.size() == 2 &&
                strCommand[0] == 'j') {
                findResult.needMoreChars = false;
                findResult.commandWithoutGroups = strCommand;
                findResult.searchPath += "(j.)";
            } else {
                findResult.searchPath += "(Unknown)";

                // Didn't find anything, return sanitized text for possible input
                auto itr = strCommand.begin();
                auto token = string_slurp_if(itr, strCommand.end(), '<', '>');
                if (token.empty()) {
                    token = strCommand;
                }
                findResult.commandWithoutGroups = token;
            }
        }
    }
}

} // namespace Zep
