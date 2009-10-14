/*
Copyright (C) 2001-2006, William Joseph.
All Rights Reserved.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "archive.h"

#include "AutoPtr.h"
#include "idatastream.h"
#include "iarchive.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "stream/filestream.h"
#include "stream/textfilestream.h"
#include "string/string.h"
#include "os/path.h"
#include "os/file.h"
#include "os/dir.h"
#include "archivelib.h"
#include "fs_path.h"


class DirectoryArchive : public Archive {
	std::string m_root;
public:
	DirectoryArchive(const std::string& root) : m_root(root) {
	}

	virtual ArchiveFile* openFile(const std::string& name) {
		UnixPath path(m_root.c_str());
		path.push_filename(name);
		AutoPtr<DirectoryArchiveFile> file(new DirectoryArchiveFile(name, path.c_str()));
		if (!file->failed()) {
			return file.release();
		}
		return 0;
	}
	virtual ArchiveTextFile* openTextFile(const std::string& name) {
		UnixPath path(m_root.c_str());
		path.push_filename(name);
		AutoPtr<DirectoryArchiveTextFile> file(new DirectoryArchiveTextFile(name, path.c_str()));
		if (!file->failed()) {
			return file.release();
		}

		UnixPath abspath("");
		abspath.push_filename(name);
		AutoPtr<DirectoryArchiveTextFile> absfile(new DirectoryArchiveTextFile(name, abspath.c_str()));
		if (!absfile->failed()) {
			return absfile.release();
		}
		return 0;
	}
	virtual bool containsFile(const std::string& name) {
		UnixPath path(m_root.c_str());
		path.push_filename(name);
		return file_readable(path.c_str());
	}
	virtual void forEachFile(VisitorFunc visitor, const std::string& root) {
		std::vector<Directory*> dirs;
		UnixPath path(m_root.c_str());
		path.push(root);
		dirs.push_back(directory_open(path.c_str()));

		while (!dirs.empty() && directory_good(dirs.back())) {
			const char* name = directory_read_and_increment(dirs.back());

			if (name == 0) {
				directory_close(dirs.back());
				dirs.pop_back();
				path.pop();
			} else if (!string_equal(name, ".") && !string_equal(name, "..")) {
				path.push_filename(name);

				bool is_directory = file_is_directory(path.c_str());

				if (!is_directory)
					visitor.file(path_make_relative(path.c_str(), m_root.c_str()));

				path.pop();

				if (is_directory) {
					path.push(name);

					if (!visitor.directory(path_make_relative(path.c_str(), m_root.c_str()), dirs.size()))
						dirs.push_back(directory_open(path.c_str()));
					else
						path.pop();
				}
			}
		}
	}
};

Archive* OpenDirArchive(const std::string& name) {
	return new DirectoryArchive(name);
}
