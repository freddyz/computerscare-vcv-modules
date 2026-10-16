#!/usr/bin/env python3
"""Derive a hidden, monitor-independent Window constructor from matching Rack source.

Keep Rack's exact private Internal layout rather than duplicating an SDK ABI.
Only the preview executable gets this constructor; Rack itself is never edited.
"""
from pathlib import Path
import re
import sys

source = (Path(sys.argv[1]) / 'src/window/Window.cpp').read_text()
internal = re.search(r'struct Window::Internal \{.*?\n\};', source, re.S)
start = source.index('Window::Window() {')
end = source.index('\n\nWindow::~Window()', start)
constructor = source[start:end]
constructor = re.sub(r'\tfloat contentScale;.*?(?=\tglfwSetWindowUserPointer)', '', constructor, flags=re.S)
constructor = re.sub(r'\tconst GLFWvidmode\* monitorMode.*?(?=\t// Set window callbacks)', '\tinternal->monitorRefreshRate = 60;\n\n', constructor, flags=re.S)
constructor = re.sub(r'\t// Set window callbacks.*?(?=\t// Set up GLEW)', '', constructor, flags=re.S)
constructor = re.sub(r'\t\tosdialog_message\([^\n]*\);\n', '', constructor)
if not internal or 'glfwShowWindow' in constructor or 'glfwGetVideoMode' in constructor:
    raise SystemExit('Unsupported Rack Window implementation; review preview initialization')
output = '''// Generated for the development renderer only. Do not edit Rack's source.
#include <rack.hpp>
#include <app/Scene.hpp>
#include <window/Window.hpp>
#include <map>
namespace rack { namespace window {
'''+internal.group(0)+'\n'+constructor+'\n} }\n'
Path(sys.argv[2]).write_text(output)
