# glib-lsp
LSP commenting things I should do while writing C code using glib

## Setup neovim
```
-- Use c LSPs for h files
vim.g.c_syntax_for_h = true

local gliblsp = vim.lsp.start_client{
  name = "glib-lsp",
  cmd = { "glib-lsp" }
}
-- For now the LSP only offers suggestions, so no keybindings needed
vim.api.nvim_create_autocmd("FileType", {
  pattern = "c",
  callback = function()
    vim.lsp.buf_attach_client(0, gliblsp)
  end,
})
```
