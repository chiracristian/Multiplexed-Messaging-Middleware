#ifndef TOPIC_TRIE_HPP
#define TOPIC_TRIE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct TopicTrieNode {
    std::unordered_map<std::string, TopicTrieNode*> children;
    
    // Set of IDs subscribed to the pattern ending here
    std::unordered_set<std::string> subscriber_ids;

    TopicTrieNode() = default;
    ~TopicTrieNode();
};

class TopicTrie
{
public:
    TopicTrie();
    ~TopicTrie();

    void add_subscription(const std::string& pattern, const std::string& client_id);
    void remove_subscription(const std::string& pattern, const std::string& client_id);
    std::unordered_set<std::string> find_subscribers(const std::string& concreteTopic);
private:
    TopicTrieNode* root;

    // Helper to split topics by '/'
    std::vector<std::string> split_topic(const std::string& topic);

    // Recursive helpers
    void collect_matches_recursive(TopicTrieNode* node, const std::vector<std::string>& tokens,
                                   size_t index, std::unordered_set<std::string>& results);
    bool remove_recursive(TopicTrieNode* node, const std::vector<std::string>& tokens, size_t index,
                          const std::string& client_id);
};

#endif  // TOPIC_TRIE_HPP
