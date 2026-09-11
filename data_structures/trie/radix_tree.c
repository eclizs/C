/**
 * @file
 * @brief A radix tree for storing and searching ASCII strings.
 * @details A radix tree compresses chains of single-child trie nodes into one
 * string prefix. See https://en.wikipedia.org/wiki/Radix_tree.
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_CHARS 128        ///< Number of supported ASCII characters.
#define MAX_WORD_LENGTH 999  ///< Maximum word length used while printing.

/** @brief A compressed node in a radix tree. */
typedef struct Node
{
    struct Node *children[NUM_CHARS];  ///< Children indexed by ASCII value.
    char *prefix;                      ///< Compressed path represented here.
    bool is_end_of_word;               ///< Whether this path is a stored word.
} Node;

/** @brief A radix tree and its root node. */
typedef struct RadixTree
{
    Node *root;  ///< Empty-prefix root, or `NULL` for an empty tree.
} RadixTree;

/**
 * @brief Allocates a node containing a copy of a prefix.
 * @param prefix Null-terminated prefix to copy.
 * @returns A zero-initialized node.
 */
static Node *create_node(const char *prefix)
{
    Node *node = calloc(1, sizeof(*node));
    if (node == NULL)
    {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    const size_t prefix_length = strlen(prefix);
    node->prefix = malloc(prefix_length + 1);
    if (node->prefix == NULL)
    {
        perror("malloc");
        free(node);
        exit(EXIT_FAILURE);
    }
    memcpy(node->prefix, prefix, prefix_length + 1);
    return node;
}

/**
 * @brief Finds the number of matching characters at the start of two strings.
 * @param first First null-terminated string.
 * @param second Second null-terminated string.
 * @returns Length of the common prefix.
 */
static size_t common_prefix_length(const char *first, const char *second)
{
    size_t length = 0;
    while (first[length] != '\0' && second[length] != '\0' &&
           first[length] == second[length])
    {
        length++;
    }
    return length;
}

/**
 * @brief Checks whether a string contains only supported ASCII bytes.
 * @param string String to validate.
 * @returns `true` when every byte can index the children array.
 */
static bool has_valid_characters(const char *string)
{
    if (string == NULL)
    {
        return false;
    }

    const unsigned char *character = (const unsigned char *)string;
    while (*character != '\0')
    {
        if (*character >= NUM_CHARS)
        {
            return false;
        }
        character++;
    }
    return true;
}

/**
 * @brief Inserts one nonempty ASCII word into a radix tree.
 * @param root Address of the tree root. An empty root is created as needed.
 * @param word Word to insert. Invalid and empty strings are ignored.
 */
void radix_insert(Node **root, const char *word)
{
    if (root == NULL || !has_valid_characters(word) || word[0] == '\0')
    {
        return;
    }
    if (*root == NULL)
    {
        *root = create_node("");
    }

    Node *node = *root;
    size_t offset = 0;
    const size_t word_length = strlen(word);

    while (offset < word_length)
    {
        const unsigned char key = (unsigned char)word[offset];
        Node *child = node->children[key];
        if (child == NULL)
        {
            child = create_node(word + offset);
            child->is_end_of_word = true;
            node->children[key] = child;
            return;
        }

        const size_t child_length = strlen(child->prefix);
        const size_t common_length =
            common_prefix_length(word + offset, child->prefix);

        if (common_length == child_length)
        {
            offset += common_length;
            node = child;
            if (offset == word_length)
            {
                node->is_end_of_word = true;
            }
            continue;
        }

        Node *old_suffix = create_node(child->prefix + common_length);
        old_suffix->is_end_of_word = child->is_end_of_word;
        memcpy(old_suffix->children, child->children,
               sizeof(old_suffix->children));

        char *shared_prefix = malloc(common_length + 1);
        if (shared_prefix == NULL)
        {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        memcpy(shared_prefix, child->prefix, common_length);
        shared_prefix[common_length] = '\0';
        free(child->prefix);
        child->prefix = shared_prefix;
        memset(child->children, 0, sizeof(child->children));
        child->children[(unsigned char)old_suffix->prefix[0]] = old_suffix;

        offset += common_length;
        child->is_end_of_word = offset == word_length;
        if (offset < word_length)
        {
            Node *new_suffix = create_node(word + offset);
            new_suffix->is_end_of_word = true;
            child->children[(unsigned char)new_suffix->prefix[0]] = new_suffix;
        }
        return;
    }
}

/**
 * @brief Finds the node containing a requested prefix.
 * @param root Root of the tree.
 * @param prefix Prefix to locate.
 * @param buffer Receives the complete path through the returned node.
 * @param capacity Number of bytes available in `buffer`.
 * @returns Matching node, or `NULL` when no match exists or the buffer is too
 * small.
 */
static Node *find_prefix_node(Node *root, const char *prefix, char *buffer,
                              size_t capacity)
{
    if (root == NULL || buffer == NULL || capacity == 0 ||
        !has_valid_characters(prefix))
    {
        return NULL;
    }

    Node *node = root;
    size_t offset = 0;
    const size_t prefix_length = strlen(prefix);
    buffer[0] = '\0';

    while (offset < prefix_length)
    {
        const unsigned char key = (unsigned char)prefix[offset];
        Node *child = node->children[key];
        if (child == NULL)
        {
            return NULL;
        }

        const size_t child_length = strlen(child->prefix);
        if (child_length >= capacity - offset)
        {
            return NULL;
        }

        const size_t common_length =
            common_prefix_length(prefix + offset, child->prefix);
        const size_t remaining_length = prefix_length - offset;
        if (common_length != remaining_length &&
            common_length != child_length)
        {
            return NULL;
        }

        memcpy(buffer + offset, child->prefix, child_length + 1);
        if (common_length == remaining_length)
        {
            return child;
        }

        node = child;
        offset += child_length;
    }
    return node;
}

/**
 * @brief Checks whether an exact word is stored in the tree.
 * @param root Root of the tree.
 * @param word Word to search for.
 * @returns `true` only when `word` has been inserted and not deleted.
 */
bool radix_search(Node *root, const char *word)
{
    if (root == NULL || !has_valid_characters(word) || word[0] == '\0')
    {
        return false;
    }

    Node *node = root;
    size_t offset = 0;
    const size_t word_length = strlen(word);
    while (offset < word_length)
    {
        node = node->children[(unsigned char)word[offset]];
        if (node == NULL)
        {
            return false;
        }

        const size_t node_length = strlen(node->prefix);
        if (common_prefix_length(word + offset, node->prefix) != node_length)
        {
            return false;
        }
        offset += node_length;
    }
    return offset == word_length && node->is_end_of_word;
}

/**
 * @brief Recursively prints every word below a node.
 * @param node Current node.
 * @param buffer Buffer containing the current complete path.
 * @param length Current path length.
 * @param capacity Number of bytes in `buffer`.
 */
static void radix_print_recursive(Node *node, char *buffer, size_t length,
                                  size_t capacity)
{
    if (node == NULL)
    {
        return;
    }
    if (node->is_end_of_word)
    {
        buffer[length] = '\0';
        puts(buffer);
    }

    for (size_t i = 0; i < NUM_CHARS; i++)
    {
        Node *child = node->children[i];
        if (child == NULL)
        {
            continue;
        }

        const size_t prefix_length = strlen(child->prefix);
        if (prefix_length >= capacity - length)
        {
            continue;
        }
        memcpy(buffer + length, child->prefix, prefix_length + 1);
        radix_print_recursive(child, buffer, length + prefix_length, capacity);
    }
}

/**
 * @brief Prints every stored word beginning with a prefix in ASCII order.
 * @param root Root of the tree.
 * @param prefix Prefix used to select words.
 */
void radix_print(Node *root, const char *prefix)
{
    char buffer[MAX_WORD_LENGTH + 1];
    Node *node = find_prefix_node(root, prefix, buffer, sizeof(buffer));
    if (node == NULL)
    {
        puts("No matches found");
        return;
    }
    radix_print_recursive(node, buffer, strlen(buffer), sizeof(buffer));
}

/**
 * @brief Checks whether a node has at least one child.
 * @param node Node to inspect.
 * @returns `true` when the node has a child.
 */
static bool node_has_children(const Node *node)
{
    if (node == NULL)
    {
        return false;
    }
    for (size_t i = 0; i < NUM_CHARS; i++)
    {
        if (node->children[i] != NULL)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Recursively removes a word and prunes empty nodes.
 * @param node Current node.
 * @param word Unmatched suffix of the word.
 * @param deleted Receives whether a stored word was removed.
 * @returns The current node, or `NULL` when it was pruned.
 */
static Node *radix_delete_recursive(Node *node, const char *word,
                                    bool *deleted)
{
    if (node == NULL)
    {
        return NULL;
    }

    const size_t node_prefix_length = strlen(node->prefix);
    if (strlen(word) < node_prefix_length ||
        strncmp(node->prefix, word, node_prefix_length) != 0)
    {
        return node;
    }
    word += node_prefix_length;

    if (*word == '\0')
    {
        if (node->is_end_of_word)
        {
            node->is_end_of_word = false;
            *deleted = true;
        }
    }
    else
    {
        const unsigned char index = (unsigned char)word[0];
        node->children[index] =
            radix_delete_recursive(node->children[index], word, deleted);
    }

    if (*deleted && !node->is_end_of_word && !node_has_children(node))
    {
        free(node->prefix);
        free(node);
        return NULL;
    }
    return node;
}

/**
 * @brief Deletes one exact word from a radix tree.
 * @param root Address of the tree root.
 * @param word Word to delete.
 * @returns `true` when the word existed and was deleted.
 */
bool radix_delete(Node **root, const char *word)
{
    if (root == NULL || *root == NULL || !has_valid_characters(word) ||
        word[0] == '\0')
    {
        return false;
    }

    bool deleted = false;
    *root = radix_delete_recursive(*root, word, &deleted);
    return deleted;
}

/**
 * @brief Releases a complete radix tree.
 * @param node Root of the subtree to release.
 */
static void free_tree(Node *node)
{
    if (node == NULL)
    {
        return;
    }
    for (size_t i = 0; i < NUM_CHARS; i++)
    {
        free_tree(node->children[i]);
    }
    free(node->prefix);
    free(node);
}

/** @brief Runs insertion, search, prefix, validation, and deletion tests. */
static void test(void)
{
    RadixTree tree = {NULL};
    const char *words[] = {"romane", "romanus", "romulus", "rubens",
                           "ruber",  "rubicon", "rubicundus"};
    const size_t word_count = sizeof(words) / sizeof(words[0]);

    for (size_t i = 0; i < word_count; i++)
    {
        radix_insert(&tree.root, words[i]);
        assert(radix_search(tree.root, words[i]));
    }
    assert(!radix_search(tree.root, "roman"));
    assert(!radix_search(tree.root, "rubicund"));
    assert(!radix_search(tree.root, "unknown"));

    /* Verify insertion of a word ending at an existing internal node. */
    radix_insert(&tree.root, "rom");
    assert(radix_search(tree.root, "rom"));
    assert(radix_search(tree.root, "romane"));
    radix_insert(&tree.root, "rom");
    assert(radix_search(tree.root, "rom"));

    char prefix_buffer[MAX_WORD_LENGTH + 1];
    (void)prefix_buffer;
    assert(find_prefix_node(tree.root, "rubi", prefix_buffer,
                            sizeof(prefix_buffer)) != NULL);
    assert(strncmp(prefix_buffer, "rubi", 4) == 0);
    assert(find_prefix_node(tree.root, "xyz", prefix_buffer,
                            sizeof(prefix_buffer)) == NULL);

    assert(!radix_delete(&tree.root, "rubicund"));
    assert(radix_search(tree.root, "rubicundus"));
    assert(radix_delete(&tree.root, "romulus"));
    assert(!radix_search(tree.root, "romulus"));
    assert(radix_search(tree.root, "romane"));
    assert(radix_delete(&tree.root, "rom"));
    assert(!radix_search(tree.root, "rom"));
    assert(radix_search(tree.root, "romane"));

    const char invalid_word[] = {(char)0x80, '\0'};
    radix_insert(&tree.root, invalid_word);
    assert(!radix_search(tree.root, invalid_word));
    assert(!radix_delete(&tree.root, invalid_word));

    puts("Words beginning with \"rubi\":");
    radix_print(tree.root, "rubi");
    free_tree(tree.root);
    puts("All tests have successfully passed!");
}

/**
 * @brief Runs the radix tree self-tests.
 * @returns `0` after all tests pass.
 */
int main(void)
{
    test();
    return 0;
}
