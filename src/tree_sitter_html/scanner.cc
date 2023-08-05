#include <tree_sitter/parser.h>
#include <algorithm>
#include <vector>
#include <string>
#include <cwctype>
#include <cstring>
#include "tag.h"

namespace
{

  using std::string;
  using std::vector;

  enum TokenType
  {
    TEXT_FRAGMENT,
    INTERPOLATION_TEXT,
    START_TAG_NAME,
    TEMPLATE_START_TAG_NAME,
    SCRIPT_START_TAG_NAME,
    STYLE_START_TAG_NAME,
    END_TAG_NAME,
    ERRONEOUS_END_TAG_NAME,
    SELF_CLOSING_TAG_DELIMITER,
    IMPLICIT_END_TAG,
    RAW_TEXT,
    COMMENT,

    AUTOMATIC_SEMICOLON,
    TEMPLATE_CHARS,
    TERNARY_QMARK,
  };

  enum JAVASCRIPT_TOKENS
  {
  };

  static void advance(TSLexer *lexer) { lexer->advance(lexer, false); }
  static void skip(TSLexer *lexer) { lexer->advance(lexer, true); }

  static bool scan_template_chars(TSLexer *lexer)
  {
    lexer->result_symbol = TEMPLATE_CHARS;
    for (bool has_content = false;; has_content = true)
    {
      lexer->mark_end(lexer);
      switch (lexer->lookahead)
      {
      case '`':
        return has_content;
      case '\0':
        return false;
      case '$':
        advance(lexer);
        if (lexer->lookahead == '{')
          return has_content;
        break;
      case '\\':
        return has_content;
      default:
        advance(lexer);
      }
    }
  }

  static bool scan_whitespace_and_comments(TSLexer *lexer)
  {
    for (;;)
    {
      while (iswspace(lexer->lookahead))
      {
        skip(lexer);
      }

      if (lexer->lookahead == '/')
      {
        skip(lexer);

        if (lexer->lookahead == '/')
        {
          skip(lexer);
          while (lexer->lookahead != 0 && lexer->lookahead != '\n')
          {
            skip(lexer);
          }
        }
        else if (lexer->lookahead == '*')
        {
          skip(lexer);
          while (lexer->lookahead != 0)
          {
            if (lexer->lookahead == '*')
            {
              skip(lexer);
              if (lexer->lookahead == '/')
              {
                skip(lexer);
                break;
              }
            }
            else
            {
              skip(lexer);
            }
          }
        }
        else
        {
          return false;
        }
      }
      else
      {
        return true;
      }
    }
  }

  static bool scan_automatic_semicolon(TSLexer *lexer)
  {
    lexer->result_symbol = AUTOMATIC_SEMICOLON;
    lexer->mark_end(lexer);

    for (;;)
    {
      if (lexer->lookahead == 0)
        return true;
      if (lexer->lookahead == '}')
        return true;
      if (lexer->is_at_included_range_start(lexer))
        return true;
      if (lexer->lookahead == '\n')
        break;
      if (!iswspace(lexer->lookahead))
        return false;
      skip(lexer);
    }

    skip(lexer);

    if (!scan_whitespace_and_comments(lexer))
      return false;

    switch (lexer->lookahead)
    {
    case ',':
    case '.':
    case ':':
    case ';':
    case '*':
    case '%':
    case '>':
    case '<':
    case '=':
    case '[':
    case '(':
    case '?':
    case '^':
    case '|':
    case '&':
    case '/':
      return false;

    // Insert a semicolon before `--` and `++`, but not before binary `+` or `-`.
    case '+':
      skip(lexer);
      return lexer->lookahead == '+';
    case '-':
      skip(lexer);
      return lexer->lookahead == '-';

    // Don't insert a semicolon before `!=`, but do insert one before a unary `!`.
    case '!':
      skip(lexer);
      return lexer->lookahead != '=';

    // Don't insert a semicolon before `in` or `instanceof`, but do insert one
    // before an identifier.
    case 'i':
      skip(lexer);

      if (lexer->lookahead != 'n')
        return true;
      skip(lexer);

      if (!iswalpha(lexer->lookahead))
        return false;

      for (unsigned i = 0; i < 8; i++)
      {
        if (lexer->lookahead != "stanceof"[i])
          return true;
        skip(lexer);
      }

      if (!iswalpha(lexer->lookahead))
        return false;
      break;
    }

    return true;
  }

  static bool scan_ternary_qmark(TSLexer *lexer)
  {
    for (;;)
    {
      if (!iswspace(lexer->lookahead))
        break;
      skip(lexer);
    }

    if (lexer->lookahead == '?')
    {
      advance(lexer);

      if (lexer->lookahead == '?')
        return false;

      lexer->mark_end(lexer);
      lexer->result_symbol = TERNARY_QMARK;

      if (lexer->lookahead == '.')
      {
        advance(lexer);
        if (iswdigit(lexer->lookahead))
          return true;
        return false;
      }
      return true;
    }
    return false;
  }

  bool tree_sitter_javascript_external_scanner_scan(TSLexer *lexer,
                                                    const bool *valid_symbols)
  {
    if (valid_symbols[TEMPLATE_CHARS])
    {
      if (valid_symbols[AUTOMATIC_SEMICOLON])
        return false;
      return scan_template_chars(lexer);
    }
    else if (valid_symbols[AUTOMATIC_SEMICOLON])
    {
      bool ret = scan_automatic_semicolon(lexer);
      if (!ret && valid_symbols[TERNARY_QMARK] && lexer->lookahead == '?')
        return scan_ternary_qmark(lexer);
      return ret;
    }
    if (valid_symbols[TERNARY_QMARK])
    {
      return scan_ternary_qmark(lexer);
    }

    return false;
  }

  struct Scanner
  {
    Scanner() {}

    unsigned serialize(char *buffer)
    {
      uint16_t tag_count = tags.size() > UINT16_MAX ? UINT16_MAX : tags.size();
      uint16_t serialized_tag_count = 0;

      unsigned i = sizeof(tag_count);
      std::memcpy(&buffer[i], &tag_count, sizeof(tag_count));
      i += sizeof(tag_count);

      for (; serialized_tag_count < tag_count; serialized_tag_count++)
      {
        Tag &tag = tags[serialized_tag_count];
        if (tag.type == CUSTOM)
        {
          unsigned name_length = tag.custom_tag_name.size();
          if (name_length > UINT8_MAX)
            name_length = UINT8_MAX;
          if (i + 2 + name_length >= TREE_SITTER_SERIALIZATION_BUFFER_SIZE)
            break;
          buffer[i++] = static_cast<char>(tag.type);
          buffer[i++] = name_length;
          tag.custom_tag_name.copy(&buffer[i], name_length);
          i += name_length;
        }
        else
        {
          if (i + 1 >= TREE_SITTER_SERIALIZATION_BUFFER_SIZE)
            break;
          buffer[i++] = static_cast<char>(tag.type);
        }
      }

      std::memcpy(&buffer[0], &serialized_tag_count, sizeof(serialized_tag_count));
      return i;
    }

    void deserialize(const char *buffer, unsigned length)
    {
      tags.clear();
      if (length > 0)
      {
        unsigned i = 0;
        uint16_t tag_count, serialized_tag_count;

        std::memcpy(&serialized_tag_count, &buffer[i], sizeof(serialized_tag_count));
        i += sizeof(serialized_tag_count);

        std::memcpy(&tag_count, &buffer[i], sizeof(tag_count));
        i += sizeof(tag_count);

        tags.resize(tag_count);
        for (unsigned j = 0; j < serialized_tag_count; j++)
        {
          Tag &tag = tags[j];
          tag.type = static_cast<TagType>(buffer[i++]);
          if (tag.type == CUSTOM)
          {
            uint16_t name_length = static_cast<uint8_t>(buffer[i++]);
            tag.custom_tag_name.assign(&buffer[i], &buffer[i + name_length]);
            i += name_length;
          }
        }
      }
    }

    string scan_tag_name(TSLexer *lexer)
    {
      string tag_name;
      while (iswalnum(lexer->lookahead) ||
             lexer->lookahead == '-' ||
             lexer->lookahead == ':')
      {
        tag_name += towupper(lexer->lookahead);
        lexer->advance(lexer, false);
      }
      return tag_name;
    }

    bool scan_comment(TSLexer *lexer)
    {
      if (lexer->lookahead != '-')
        return false;
      lexer->advance(lexer, false);
      if (lexer->lookahead != '-')
        return false;
      lexer->advance(lexer, false);

      unsigned dashes = 0;
      while (lexer->lookahead)
      {
        switch (lexer->lookahead)
        {
        case '-':
          ++dashes;
          break;
        case '>':
          if (dashes >= 2)
          {
            lexer->result_symbol = COMMENT;
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            return true;
          }
        default:
          dashes = 0;
        }
        lexer->advance(lexer, false);
      }
      return false;
    }

    bool scan_raw_text(TSLexer *lexer)
    {
      if (!tags.size())
        return false;

      lexer->mark_end(lexer);

      const string &end_delimiter = tags.back().type == SCRIPT
                                        ? "</SCRIPT"
                                        : "</STYLE";

      unsigned delimiter_index = 0;
      while (lexer->lookahead)
      {
        if (towupper(lexer->lookahead) == end_delimiter[delimiter_index])
        {
          delimiter_index++;
          if (delimiter_index == end_delimiter.size())
            break;
          lexer->advance(lexer, false);
        }
        else
        {
          delimiter_index = 0;
          lexer->advance(lexer, false);
          lexer->mark_end(lexer);
        }
      }

      lexer->result_symbol = RAW_TEXT;
      return true;
    }

    bool scan_implicit_end_tag(TSLexer *lexer)
    {
      Tag *parent = tags.empty() ? NULL : &tags.back();

      bool is_closing_tag = false;
      if (lexer->lookahead == '/')
      {
        is_closing_tag = true;
        lexer->advance(lexer, false);
      }
      else
      {
        if (parent && parent->is_void())
        {
          tags.pop_back();
          lexer->result_symbol = IMPLICIT_END_TAG;
          return true;
        }
      }

      string tag_name = scan_tag_name(lexer);
      if (tag_name.empty())
        return false;

      Tag next_tag = Tag::for_name(tag_name);

      if (is_closing_tag)
      {
        // The tag correctly closes the topmost element on the stack
        if (!tags.empty() && tags.back() == next_tag)
          return false;

        // Otherwise, dig deeper and queue implicit end tags (to be nice in
        // the case of malformed HTML)
        if (std::find(tags.begin(), tags.end(), next_tag) != tags.end())
        {
          tags.pop_back();
          lexer->result_symbol = IMPLICIT_END_TAG;
          return true;
        }
      }
      else if (parent && !parent->can_contain(next_tag))
      {
        tags.pop_back();
        lexer->result_symbol = IMPLICIT_END_TAG;
        return true;
      }

      return false;
    }

    bool scan_start_tag_name(TSLexer *lexer)
    {
      string tag_name = scan_tag_name(lexer);
      if (tag_name.empty())
        return false;
      Tag tag = Tag::for_name(tag_name);
      tags.push_back(tag);
      switch (tag.type)
      {
      case TEMPLATE:
        lexer->result_symbol = TEMPLATE_START_TAG_NAME;
        break;
      case SCRIPT:
        lexer->result_symbol = SCRIPT_START_TAG_NAME;
        break;
      case STYLE:
        lexer->result_symbol = STYLE_START_TAG_NAME;
        break;
      default:
        lexer->result_symbol = START_TAG_NAME;
        break;
      }
      return true;
    }

    bool scan_end_tag_name(TSLexer *lexer)
    {
      string tag_name = scan_tag_name(lexer);
      if (tag_name.empty())
        return false;
      Tag tag = Tag::for_name(tag_name);
      if (!tags.empty() && tags.back() == tag)
      {
        tags.pop_back();
        lexer->result_symbol = END_TAG_NAME;
      }
      else
      {
        lexer->result_symbol = ERRONEOUS_END_TAG_NAME;
      }
      return true;
    }

    bool scan_self_closing_tag_delimiter(TSLexer *lexer)
    {
      lexer->advance(lexer, false);
      if (lexer->lookahead == '>')
      {
        lexer->advance(lexer, false);
        if (!tags.empty())
        {
          tags.pop_back();
          lexer->result_symbol = SELF_CLOSING_TAG_DELIMITER;
        }
        return true;
      }
      return false;
    }

    bool scan(TSLexer *lexer, const bool *valid_symbols)
    {
      // tree_sitter_javascript_external_scanner_scan(lexer, valid_symbols);
      while (iswspace(lexer->lookahead))
      {
        lexer->advance(lexer, true);
      }

      if (valid_symbols[TEMPLATE_CHARS])
      {
        if (valid_symbols[AUTOMATIC_SEMICOLON])
          return false;
        return scan_template_chars(lexer);
      }
      else if (valid_symbols[AUTOMATIC_SEMICOLON])
      {
        bool ret = scan_automatic_semicolon(lexer);
        if (!ret && valid_symbols[TERNARY_QMARK] && lexer->lookahead == '?')
          return scan_ternary_qmark(lexer);
        return ret;
      }
      if (valid_symbols[TERNARY_QMARK])
      {
        return scan_ternary_qmark(lexer);
      }
      if (valid_symbols[RAW_TEXT] && !valid_symbols[START_TAG_NAME] && !valid_symbols[END_TAG_NAME])
      {
        return scan_raw_text(lexer);
        // return tree_sitter_javascript_external_scanner_scan(lexer, valid_symbols);
      }

      switch (lexer->lookahead)
      {
      case '<':
        lexer->mark_end(lexer);
        lexer->advance(lexer, false);

        if (lexer->lookahead == '!')
        {
          lexer->advance(lexer, false);
          return scan_comment(lexer);
        }

        if (valid_symbols[IMPLICIT_END_TAG])
        {
          return scan_implicit_end_tag(lexer);
        }
        break;

      case '\0':
        if (valid_symbols[IMPLICIT_END_TAG])
        {
          return scan_implicit_end_tag(lexer);
        }
        break;

      case '/':
        if (valid_symbols[SELF_CLOSING_TAG_DELIMITER])
        {
          return scan_self_closing_tag_delimiter(lexer);
        }
        break;

      default:
        if ((valid_symbols[START_TAG_NAME] || valid_symbols[END_TAG_NAME]) && !valid_symbols[RAW_TEXT])
        {
          return valid_symbols[START_TAG_NAME]
                     ? scan_start_tag_name(lexer)
                     : scan_end_tag_name(lexer);
        }
      }

      return false;
    }

    vector<Tag> tags;
  };

}

extern "C"
{

  void *tree_sitter_html_external_scanner_create()
  {
    return new Scanner();
  }

  bool tree_sitter_html_external_scanner_scan(void *payload, TSLexer *lexer,
                                              const bool *valid_symbols)
  {
    Scanner *scanner = static_cast<Scanner *>(payload);
    return scanner->scan(lexer, valid_symbols);
  }

  unsigned tree_sitter_html_external_scanner_serialize(void *payload, char *buffer)
  {
    Scanner *scanner = static_cast<Scanner *>(payload);
    return scanner->serialize(buffer);
  }

  void tree_sitter_html_external_scanner_deserialize(void *payload, const char *buffer, unsigned length)
  {
    Scanner *scanner = static_cast<Scanner *>(payload);
    scanner->deserialize(buffer, length);
  }

  void tree_sitter_html_external_scanner_destroy(void *payload)
  {
    Scanner *scanner = static_cast<Scanner *>(payload);
    delete scanner;
  }
}
