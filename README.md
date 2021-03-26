# fsearch
Advanced File Search (POSIX Compatible)

FSearch is advanced file search program for POSIX compatible operating systems. It is able to
search for files satisfying the given criteria and display the results in the form of nicely formatted
tree.

### Installation
Installation of this program is possible with Makefile.

```
git clone https://github.com/kala13x/fsearch.git
cd fsearch
make
sudo make install
```

### Usage
```
fsearch [-i <indentation>] [-f <file_name>] [-b <file_size>]
        [-p <permissions>] [-t <file_type>] [-o <file_path>]
        [-d <target_path>] [-l <link_count>] [-r] [-v] [-h]
```

#### Options:
```
  -d <target_path>    # Target directory path
  -i <indentation>    # Ident using tabs with specified size
  -o <file_path>      # Write output in a specified file
  -f <file_name>      # Target file name (case insensitive)
  -b <file_size>      # Target file size in bytes
  -t <file_type>      # Target file type
  -l <link_count>     # Target file link count
  -p <permissions>    # Target file permissions (e.g. 'rwxr-xr--')
  -r                  # Recursive search target directory
  -v                  # Display additional information (verbose) 
  -h                  # Displays version and usage information
```

#### File types:
```
   b: block device
   c: character device
   d: directory
   f: regular file
   l: symbolic link
   p: pipe
   s: socket
```

#### Notes:
   1) `<filename>` option is supporting the following regular expression: `+`
   2) `<file_type>` option is supporting one and more file types like: `-t ldb`

#### Example:
```
fsearch -d targetDirectoryPath -f lost+file -b 100 -t b
```

### Output

Example of the recursive search output (`-r` option):
```
./project/shared/cpr/test/data/ca.key
./project/shared/cpr/test/data/client.key
./project/shared/cpr/test/data/server.key
./stuff/certs/server.key
```

Example of the nicely formated output (`-i` option):
```
.
|---project
|------shared
|---------cpr
|------------test
|---------------data
|------------------ca.key
|------------------client.key
|------------------server.key
|---stuff
|------certs
|---------server.key
```

Example of the verbosed output (`-v` option):
```
-rw-r--r--  1  kala  kala        3468 [Mar  9 03:20] ./project/shared/cpr/test/data/ca.key
-rw-r--r--  1  kala  kala        3294 [Mar  9 03:20] ./project/shared/cpr/test/data/client.key
-rw-r--r--  1  kala  kala        3294 [Mar  9 03:20] ./project/shared/cpr/test/data/server.key
-rw-------  1  kala  kala        3243 [Mar  9 03:20] ./stuff/certs/server.key
```
