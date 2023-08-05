# tree-sitter-vue

<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/deuscx/tree-sitter-vue">
    <!-- <img src="" alt="Logo" width="80" height="80"-->
  </a>

  <h3 align="center">tree-sitter-vue</h3>

  <p align="center">
    A tree-sitter grammer for vue3
  </p>
</div>

<!-- PROJECT SHIELDS -->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![GitHub][license-shield]][license-url]

## Features

- support script content analyze

## Getting Started

![](https://raw.githubusercontent.com/Deuscx/pic/master/20230806061125.png)

## Installation

```bash
npm install @deuscx/tree-sitter-vue
```

<!-- USAGE EXAMPLES -->
## Usage

use in lazy.nvim & tree-sitter nvim

```lua
local parser_config = require "nvim-treesitter.parsers".get_parser_configs()
parser_config.vue3 = {
  install_info = {
    url = "https://github.com/Deuscx/tree-sitter-vue", -- local path or git repo
    files = {"src/parser.c", "scr/scanner.cc"}, -- note that some parsers also require src/scanner.c or src/scanner.cc
    -- optional entries:
    branch = "main", -- default branch in case of git repo if different from master
    generate_requires_npm = false, -- if stand-alone parser without npm dependencies
    requires_generate_from_grammar = false, -- if folder contains pre-generated src/parser.c
  },
  filetype = "vue", -- if filetype does not match the parser name
}
```

Then use `TSInstall vue3` to install it.

See the [open issues](https://github.com/deuscx/tree-sitter-vue/issues) for a full list of proposed features (and known issues).

<!-- LICENSE -->
## License

Distributed under the MIT License. See [LICENSE]('./LICENSE') for more information.

[contributors-shield]: https://img.shields.io/github/contributors/deuscx/tree-sitter-vue.svg?style=for-the-badge
[contributors-url]: https://github.com/deuscx/tree-sitter-vue/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/deuscx/tree-sitter-vue.svg?style=for-the-badge
[forks-url]: https://github.com/deuscx/tree-sitter-vue/network/members
[stars-shield]: https://img.shields.io/github/stars/deuscx/tree-sitter-vue.svg?style=for-the-badge
[stars-url]: https://github.com/deuscx/tree-sitter-vue/stargazers
[issues-shield]: https://img.shields.io/github/issues/deuscx/tree-sitter-vue.svg?style=for-the-badge
[issues-url]: https://github.com/deuscx/tree-sitter-vue/issues
[license-shield]: https://img.shields.io/github/license/deuscx/tree-sitter-vue?style=for-the-badge
[license-url]: https://github.com/deuscx/tree-sitter-vue/blob/master/LICENSE

## Credits & External Resources

- [tree-sitter-vue](https://github.com/ikatyang/tree-sitter-vue)
