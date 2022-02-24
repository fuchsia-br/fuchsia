// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"path"
	"path/filepath"
	"reflect"
	"sort"
	"strings"

	"github.com/google/go-cmp/cmp"
)

type flagsDef struct {
	manifest *string
	regen    *bool
}

var flags = flagsDef{
	manifest: flag.String("manifest", "", "JSON manifest"),
	regen:    flag.Bool("regen", false, "regen instead of testing"),
}

func printUsage() {
	program := path.Base(os.Args[0])
	message := `Usage: ` + program + ` [flags]

Utility used to test/regen golden files.

Flags:
`
	fmt.Fprint(flag.CommandLine.Output(), message)
	flag.PrintDefaults()
}

func main() {
	flag.Usage = printUsage
	flag.Parse()
	// We use log for errors, so clear flags to remove date/time.
	log.SetFlags(0)

	if *flags.manifest == "" {
		log.Fatal("must provide --manifest flag")
	}
	manifestJSON, err := os.ReadFile(*flags.manifest)
	if err != nil {
		log.Fatal(err)
	}
	var manifest manifest
	if err := json.Unmarshal(manifestJSON, &manifest); err != nil {
		log.Fatalf("%s: %s", *flags.manifest, err)
	}
	if err := manifest.validate(); err != nil {
		log.Fatalf("%s: %s", *flags.manifest, err)
	}

	if *flags.regen {
		if err := manifest.regen(os.Stdout); err != nil {
			log.Fatal(err)
		}
	} else {
		passed, err := manifest.test(os.Stdout)
		if err != nil {
			log.Fatal(err)
		}
		if !passed {
			fmt.Println("Run the test again with the --regen flag to regenerate goldens")
			os.Exit(1)
		}
	}
}

// A manifest stores the information needed to test/regen goldens.
type manifest struct {
	// Goldens directory used in test mode.
	TestGoldensDir string `json:"test_goldens_dir"`
	// Goldens directory used in regen mode.
	RegenGoldensDir string `json:"regen_goldens_dir"`
	// List of files to test/regen.
	Entries []entry `json:"entries"`
}

// An entry represents a file that gets compared to (in test mode) or
// overwritten by (in regen mode) another file generated by the build.
type entry struct {
	// Golden filename, relative to TestGoldensDir and RegenGoldensDir.
	Golden string `json:"golden"`
	// Path to the corresponding generated file.
	Generated string `json:"generated"`
}

func (m *manifest) validate() error {
	if m.TestGoldensDir == "" {
		return fmt.Errorf("missing test dir")
	}
	if m.RegenGoldensDir == "" {
		return fmt.Errorf("missing regen dir")
	}
	seenGolden := make(map[string]struct{}, len(m.Entries))
	seenGenerated := make(map[string]struct{}, len(m.Entries))
	for _, entry := range m.Entries {
		if entry.Golden == "" {
			return fmt.Errorf("entry missing golden path")
		}
		if entry.Generated == "" {
			return fmt.Errorf("entry missing generated path")
		}
		if _, ok := seenGolden[entry.Golden]; ok {
			return fmt.Errorf("%s: duplicate golden path", entry.Golden)
		}
		if _, ok := seenGenerated[entry.Generated]; ok {
			return fmt.Errorf("%s: duplicate generated path", entry.Generated)
		}
		seenGolden[entry.Golden] = struct{}{}
		seenGenerated[entry.Generated] = struct{}{}
		if strings.ContainsRune(entry.Golden, '/') {
			return fmt.Errorf("%s: subdirectories not allowed", entry.Golden)
		}
		if filepath.Ext(entry.Golden) != ".golden" {
			return fmt.Errorf("%s: expected .golden extension", entry.Golden)
		}
		if filepath.Ext(entry.Generated) == ".golden" {
			return fmt.Errorf("%s: unexpected .golden extension", entry.Generated)
		}
	}
	return nil
}

func (m *manifest) regen(w io.Writer) error {
	// Print the destination directory. Use an absolute path, since the provided
	// path is relative to GN's root_build_dir.
	absGoldensDir, err := filepath.Abs(m.RegenGoldensDir)
	if err != nil {
		return err
	}
	fmt.Fprintf(w, "Regenerating goldens in %s\n", absGoldensDir)

	// Read the current contents of goldens.txt into oldGoldens and newGoldens.
	// We update newGoldens after each operation so that at every point it
	// reflects the current state of the filesystem.
	goldensTxtPath := filepath.Join(m.RegenGoldensDir, "goldens.txt")
	goldensTxtFile, err := os.OpenFile(goldensTxtPath, os.O_RDWR, 0)
	if err != nil {
		return err
	}
	defer goldensTxtFile.Close()
	oldGoldens, err := readGoldensTxt(goldensTxtFile)
	if err != nil {
		return fmt.Errorf("%s: %w", goldensTxtPath, err)
	}
	newGoldens := make(map[string]struct{})
	for golden := range oldGoldens {
		newGoldens[golden] = struct{}{}
	}

	// Defer rewriting goldens.txt so that it remains accurate even if something
	// fails partway through the rest of this function.
	defer func() {
		// Avoid touching goldens.txt if it hasn't changed. This saves a lot of
		// build work since goldens.txt is read at GN gen time.
		if reflect.DeepEqual(oldGoldens, newGoldens) {
			return
		}
		goldensTxtFile.Truncate(0)
		goldensTxtFile.Seek(0, 0)
		for _, golden := range setToSortedSlice(newGoldens) {
			goldensTxtFile.WriteString(golden)
			goldensTxtFile.WriteString("\n")
		}
	}()

	manifestGoldens := make(map[string]struct{}, len(m.Entries))
	for _, entry := range m.Entries {
		goldenPath := filepath.Join(m.RegenGoldensDir, entry.Golden)
		if err := copyIfDifferent(goldenPath, entry.Generated, func() {
			fmt.Fprintf(w, "Writing %s\n", entry.Golden)
		}); err != nil {
			return err
		}
		// Record the golden. It's better to do this after copyIfDifferent and
		// risk having an untracked golden file (due to an error between create
		// and copy) than to do it before and risk having a nonexistent file in
		// goldens.txt (due to an error during create) which makes GN gen fail.
		newGoldens[entry.Golden] = struct{}{}
		manifestGoldens[entry.Golden] = struct{}{}
	}

	// Purge old goldens that are no longer used, including stray .golden files
	// that were not tracked in goldens.txt (likely a merge/rebase mistake).
	dirEntries, err := os.ReadDir(m.RegenGoldensDir)
	if err != nil {
		return err
	}
	for _, d := range dirEntries {
		if !(d.Type().IsRegular() && filepath.Ext(d.Name()) == ".golden") {
			continue
		}
		golden := d.Name()
		if _, ok := manifestGoldens[golden]; ok {
			continue
		}
		fmt.Fprintf(w, "Removing %s\n", golden)
		if err := os.Remove(filepath.Join(m.RegenGoldensDir, golden)); err != nil {
			return err
		}
		delete(newGoldens, golden)
	}

	return nil
}

func (m *manifest) test(w io.Writer) (bool, error) {
	// Read goldens.txt to ensure we only consider fresh host_test_data copies,
	// not old files that happen to remain in the build directory.
	goldensTxtPath := filepath.Join(m.TestGoldensDir, "goldens.txt")
	goldensTxtFile, err := os.Open(goldensTxtPath)
	if err != nil {
		return false, err
	}
	remainingGoldens, err := readGoldensTxt(goldensTxtFile)
	goldensTxtFile.Close()
	if err != nil {
		return false, fmt.Errorf("%s: %w", goldensTxtPath, err)
	}

	reporter := reporter{Writer: w}
	for _, entry := range m.Entries {
		tc := reporter.testCase(entry.Golden)
		tc.announce()
		if _, ok := remainingGoldens[entry.Golden]; !ok {
			tc.fail("file missing from goldens.txt (forgot to regen?)")
			continue
		}
		delete(remainingGoldens, entry.Golden)
		goldenPath := filepath.Join(m.TestGoldensDir, entry.Golden)
		goldenBytes, err := os.ReadFile(goldenPath)
		if err != nil {
			tc.fail("%s", err)
			continue
		}
		generatedPath := entry.Generated
		generatedBytes, err := os.ReadFile(generatedPath)
		if err != nil {
			tc.fail("%s", err)
			continue
		}
		if len(goldenBytes) != 0 && len(generatedBytes) == 0 {
			tc.fail("%s: generated file was unexpectedly empty", generatedPath)
			continue
		}
		goldenLines := strings.Split(string(goldenBytes), "\n")
		generatedLines := strings.Split(string(generatedBytes), "\n")
		if diff := cmp.Diff(goldenLines, generatedLines); diff != "" {
			tc.fail(`unexpected difference between golden file:
	%s
and generated file:
	%s
diff -golden +generated:
%s`,
				goldenPath, generatedPath, diff)
			continue
		}
		tc.pass()
	}

	if len(remainingGoldens) != 0 {
		var msg strings.Builder
		msg.WriteString("extra files in goldens.txt (forgot to regen?):\n")
		for _, golden := range setToSortedSlice(remainingGoldens) {
			msg.WriteString(fmt.Sprintf("\t%s\n", golden))
		}
		tc := reporter.testCase("goldens.txt")
		tc.announce()
		tc.fail(msg.String())
	}

	reporter.summarize()
	return !reporter.failed, nil
}

func readGoldensTxt(rd io.Reader) (map[string]struct{}, error) {
	scanner := bufio.NewScanner(rd)
	goldens := make(map[string]struct{})
	for scanner.Scan() {
		path := scanner.Text()
		// Omit empty lines to avoid spurious "" paths when setting up tests for
		// the first time (e.g. `touch goldens.txt` or `echo > goldens.txt`).
		if path == "" {
			continue
		}
		if strings.ContainsRune(path, '/') {
			return nil, fmt.Errorf("%s: subdirectories not allowed", path)
		}
		if filepath.Ext(path) != ".golden" {
			return nil, fmt.Errorf("%s: expected .golden extension", path)
		}
		goldens[path] = struct{}{}
	}
	if err := scanner.Err(); err != nil {
		return nil, err
	}
	return goldens, nil
}

// copyIfDifferent calls beforeCopy() and copies from dstPath to srcPath, unless
// the file contents are already the same in which case it does nothing.
func copyIfDifferent(dstPath, srcPath string, beforeCopy func()) error {
	src, err := os.Open(srcPath)
	if err != nil {
		return err
	}
	defer src.Close()
	dst, err := os.OpenFile(dstPath, os.O_RDWR|os.O_CREATE, 0o666)
	if err != nil {
		return err
	}
	defer dst.Close()
	srcInfo, err := src.Stat()
	if err != nil {
		return err
	}
	dstInfo, err := dst.Stat()
	if err != nil {
		return err
	}
	if srcInfo.Size() != dstInfo.Size() {
		beforeCopy()
		dst.Truncate(0)
		_, err = io.Copy(dst, src)
		return err
	}
	srcBytes, err := io.ReadAll(src)
	if err != nil {
		return err
	}
	dstBytes, err := io.ReadAll(dst)
	if err != nil {
		return err
	}
	if bytes.Equal(srcBytes, dstBytes) {
		return nil
	}
	beforeCopy()
	dst.Truncate(0)
	dst.Seek(0, 0)
	_, err = dst.Write(srcBytes)
	return err
}

func setToSortedSlice(set map[string]struct{}) []string {
	res := make([]string, 0, len(set))
	for s := range set {
		res = append(res, s)
	}
	sort.Strings(res)
	return res
}

type reporter struct {
	io.Writer
	failed bool
}

func (r *reporter) printf(format string, args ...interface{}) {
	fmt.Fprintf(r, format, args...)
}

func (r *reporter) summarize() {
	if r.failed {
		r.printf("FAIL\n")
	} else {
		r.printf("PASS\n")
	}
}

func (r *reporter) testCase(name string) testCase {
	return testCase{name, r}
}

type testCase struct {
	name     string
	reporter *reporter
}

func (c *testCase) announce() {
	c.reporter.printf("=== TEST: %s\n", c.name)
}

func (c *testCase) pass() {
	c.reporter.printf("--- PASS: %s\n", c.name)
}

func (c *testCase) fail(format string, args ...interface{}) {
	c.reporter.printf("--- FAIL: %s\n", c.name)
	s := fmt.Sprintf(format, args...)
	c.reporter.printf("%s", s)
	if s[len(s)-1] != '\n' {
		c.reporter.printf("\n")
	}
	c.reporter.failed = true
}
