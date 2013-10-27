/// glitch - Produce glitch effect in a JPG file.
/// 
/// Copyright (c) 2013 Oliver Lau <oliver@von-und-fuer-lau.de>.
/// All rights reserved.

#ifndef WIN32
#include <sys/time.h>
#endif

#ifdef WIN32
#include <Windows.h>
#endif

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <getopt.h>

enum _long_options {
  SELECT_HELP,
  SELECT_PERCENT,
  SELECT_IN,
  SELECT_OUT,
  SELECT_ITERATIONS,
  SELECT_QUIET,
  SELECT_VERBOSE,
  SELECT_ALGORITHM
};

enum ALGORITHMS { ALGORITHM_OR, ALGORITHM_XOR };

static struct option long_options[] = {
  { "help",       no_argument,       0, SELECT_HELP },
  { "percent",    required_argument, 0, SELECT_PERCENT },
  { "iterations", required_argument, 0, SELECT_ITERATIONS },
  { "algorithm",  required_argument, 0, SELECT_ALGORITHM },
  { "in",         required_argument, 0, SELECT_IN },
  { "out",        required_argument, 0, SELECT_OUT },
  { "quiet",      no_argument,       0, SELECT_QUIET },
  { "verbose",    no_argument,       0, SELECT_VERBOSE },
  { 0,                      0,       0, 0 }
};

static int verbose = 0;
static int iterations = 10;
static bool quiet = false;
static char *infile = NULL;
static char *outfile = NULL;
static double percent = 10;
static ALGORITHMS algorithm = ALGORITHM_XOR;
static unsigned int BUFFSIZE = 1024;

template <typename T> inline T MIN(T x, T y) { return (x > y) ? y : x; }
template <typename T> inline T MAX(T x, T y) { return (x < y) ? y : x; }

static inline bool fuzzyEqual(double x, double y) {
  return fabs(x - y) <= (0.000000000001 * MIN(fabs(x), fabs(y)));
}


double random(double x, double y) {
  if (fuzzyEqual(x, y))
    return x;
  double hi = MAX(x, y), lo = MIN(x, y);
  return lo + ((hi - lo) * rand() / (RAND_MAX + 1.0));
}


void usage() {
  printf("Usage: glitch --in original.jpg --out glitched.jpg [--iterations|-i %d] [--percent|-p %lf] [--algorithm|-a XOR|OR]\n\r", iterations, percent);
}


#ifdef WIN32
void glitch(void) {
  srand(GetTickCount());
  BOOL rc = CopyFile(infile, outfile, FALSE);
  if (rc == 0) {
    printf(TEXT("Copying failed, last error: %d\n"), GetLastError());
    exit(1);
  }
  HANDLE hFile = CreateFile(outfile, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    printf(TEXT("hFile is NULL\n"));
    printf(TEXT("Target file is %s\n"), outfile);
    exit(4);
  }
  DWORD dwFileSize = GetFileSize(hFile, NULL);
  SYSTEM_INFO SysInfo;
  GetSystemInfo(&SysInfo);
  DWORD dwSysGran = SysInfo.dwAllocationGranularity;
  DWORD dwFirstPos = (DWORD)(1e-2 * dwFileSize * percent);
  HANDLE hMapFile = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
  if (hMapFile == NULL) {
    printf(TEXT("hMapFile is NULL, last error: %d\n"), GetLastError() );
    exit(2);
  }
  if (verbose > 0)
    printf("randomly glitching in between %lu and %lu\n", dwFirstPos, dwFileSize);
  for (int i = 0; i < iterations; ++i) {
    DWORD dwPos = (DWORD) random(dwFirstPos, dwFileSize);
    DWORD dwFileMapStart = (dwPos / dwSysGran) * dwSysGran;
    LPVOID lpMapAddress = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, dwFileMapStart, 0);
    if (lpMapAddress == NULL) {
      printf(TEXT("lpMapAddress is NULL, last error: %d\n"), GetLastError());
      exit(3);
    }
    BYTE *b = (BYTE*)lpMapAddress + dwPos % dwSysGran;
    INT bit = rand() % 8;
    BYTE oldByte = *b;
    BYTE newByte = oldByte;
    switch (algorithm) {
    default:
      // fall-through
    case ALGORITHM_XOR:
      newByte ^= (1 << bit);
      break;
    case ALGORITHM_OR:
      newByte |= (1 << bit);
      break;
    }
    *b = newByte;
    if (verbose > 0)
      printf("glitching @%11lu[%d]: %02xh->%02xh\n", dwPos, bit, (UINT)oldByte, (UINT)newByte);
    UnmapViewOfFile(lpMapAddress);
  }
  CloseHandle(hMapFile);
  CloseHandle(hFile);
}
#else
void glitch(void) {
  FILE *in = fopen(infile, "rb");
  fseek(in, 0L, SEEK_END);
  long sz = ftell(in);
  fseek(in, 0L, SEEK_SET);
  unsigned char *buf = (unsigned char*) malloc(sz);
  fread(buf, 1, sz, in);
  fclose(in);
  FILE *out = fopen(outfile, "wb+");
  struct timeval tv;
  gettimeofday(&tv, 0);
  long int n = (tv.tv_sec ^ tv.tv_usec) ^ getpid();
  srand(n);
  long firstPos = (long)(1e-2 * sz * percent);
  printf("in between %lu and %lu\n\r", firstPos, sz);
  for (int i = 0; i < iterations; ++i) {
    const long pos = (long) random(firstPos, sz);
    const int bit = rand() % 8;
    const unsigned char oldByte = buf[pos];
    unsigned char newByte = oldByte;
    switch (algorithm) {
    default:
      // fall-through
    case ALGORITHM_XOR:
      newByte ^= (1 << bit);
      break;
    case ALGORITHM_OR:
      newByte |= (1 << bit);
      break;
    }
    buf[pos] = newByte;
    printf("glitching at position %ld:%d (%02xh->%02xh)\n", pos, bit, (unsigned int) oldByte, (unsigned int) newByte);
  }
  fwrite(buf, 1, sz, out);
  fclose(out);
  free(buf);
}
#endif

int main(int argc, char* argv[]) {
  for (;;) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "i:p:h?vqa:", long_options, &option_index);
    if (c == -1)
      break;
    switch (c)
    {
    case SELECT_IN:
      infile = optarg;
      break;
    case SELECT_OUT:
      outfile = optarg;
      break;
    case 'i':
    case SELECT_ITERATIONS:
      iterations = atoi(optarg);
      break;
    case 'p':
    case SELECT_PERCENT:
      percent = atof(optarg);
      break;
    case 'a':
    case SELECT_ALGORITHM:
      if (strcmp(optarg, "OR") == 0)
        algorithm = ALGORITHM_OR;
      break;
    case 'h':
      /* fall-through */
    case '?':
      /* fall-through */
    case SELECT_HELP:
      usage();
      return 0;
      break;
    case SELECT_VERBOSE:
      /* fall-through */
    case 'v':
      ++verbose;
      break;
    case SELECT_QUIET:
      /* fall-through */
    case 'q':
      quiet = true;
      break;
    default:
      usage();
      exit(EXIT_FAILURE);
      break;
    }
  }
  if (infile == NULL || outfile == NULL) {
    usage();
    return EXIT_FAILURE;
  }
  if (percent >= 100) {
    usage();
    return EXIT_FAILURE;
  }
  if (iterations < 1) {
    usage();
    return EXIT_FAILURE;
  }
  if (verbose > 1)
    printf("%s -> %s\n", infile, outfile); 
  glitch();
  return EXIT_SUCCESS;
}