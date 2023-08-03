/// <reference types="tree-sitter-cli/dsl" />
module.exports = grammar({
  name: 'vue',

  rules: {
    // TODO: add the actual grammar rules
    source_file: $ => 'hello'
  }
});