{
  "targets": [
    {
      "target_name": "gil_napi",
      "sources": [
        "src/gil_napi.cc",
        "./libgil/src/intern.c",
        "./libgil/src/lexer.c",
        "./libgil/src/parser.c",
        "./libgil/src/gil.c",
        "./libgil/src/load.c",
        "./libgil/src/frontier.c",
        "./libgil/src/query.c",
        "./libgil/src/exec.c"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "./libgil/include",
        "./libgil/src"
      ],
      "defines": [ "NAPI_CPP_EXCEPTIONS" ],
      "cflags_cc": [ "-std=c++17", "-fexceptions" ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1
        }
      }
    }
  ]
}