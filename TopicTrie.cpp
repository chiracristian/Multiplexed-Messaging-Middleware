// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025–2026 Cristian-Ioan-George Chira

#include "TopicTrie.hpp"
#include <sstream>

TopicTrieNode::~TopicTrieNode() {
    for (auto& child : children) {
        delete child.second;
    }
}

TopicTrie::TopicTrie() {
    root = new TopicTrieNode();
}

TopicTrie::~TopicTrie() {
    delete root;
}

std::vector<std::string> TopicTrie::split_topic(const std::string& topic) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(topic);

    while (std::getline(tokenStream, token, '/')) {
        tokens.push_back(token);
    }

    return tokens;
}

void TopicTrie::add_subscription(const std::string& pattern, const std::string& client_id) {
    std::vector<std::string> tokens = split_topic(pattern);
    TopicTrieNode* current = root;
    for (const auto& token : tokens) {
        if (current->children.find(token) == current->children.end()) {
            current->children[token] = new TopicTrieNode();
        }
        current = current->children[token];
    }
    current->subscriber_ids.insert(client_id);
}

void TopicTrie::remove_subscription(const std::string& pattern, const std::string& client_id) {
    std::vector<std::string> tokens = split_topic(pattern);
    remove_recursive(root, tokens, 0, client_id);
}

std::unordered_set<std::string> TopicTrie::find_subscribers(const std::string& concreteTopic) {
    std::unordered_set<std::string> results;
    std::vector<std::string> tokens = split_topic(concreteTopic);
    collect_matches_recursive(root, tokens, 0, results);
    return results;
}

void TopicTrie::collect_matches_recursive(TopicTrieNode* node, const std::vector<std::string>& tokens, 
                                          size_t index, std::unordered_set<std::string>& results) {
    // 1. Literal Match
    if (index < tokens.size()) {
        auto it = node->children.find(tokens[index]);
        if (it != node->children.end()) {
            collect_matches_recursive(it->second, tokens, index + 1, results);
        }
    } else {
        // Base case: end of topic reached, add all subscribers at this node
        results.insert(node->subscriber_ids.begin(), node->subscriber_ids.end());
    }

    // 2. Single-level Wildcard (+)
    if (index < tokens.size()) {
        auto it = node->children.find("+");
        if (it != node->children.end()) {
            collect_matches_recursive(it->second, tokens, index + 1, results);
        }
    }

    // 3. Multi-level Wildcard (*) - Backtracking
    auto it = node->children.find("*");
    if (it != node->children.end()) {
        for (size_t i = index; i <= tokens.size(); ++i) {
            collect_matches_recursive(it->second, tokens, i, results);
        }
    }
}

bool TopicTrie::remove_recursive(TopicTrieNode* node, const std::vector<std::string>& tokens, 
                                 size_t index, const std::string& client_id) {
    if (index == tokens.size()) {
        node->subscriber_ids.erase(client_id);
        // Node is eligible for pruning if no subscribers left and no children
        return node->subscriber_ids.empty() && node->children.empty();
    }

    auto it = node->children.find(tokens[index]);
    if (it != node->children.end()) {
        if (remove_recursive(it->second, tokens, index + 1, client_id)) {
            delete it->second;
            node->children.erase(it);
            return node->children.empty() && node->subscriber_ids.empty();
        }
    }
    return false;
}
