# Development / Release / Bugfix Workflows

The purpose of this document is to describe and define how changes flow into our
code and through the various stages of development until it finally goes into
production.

The general idea is based on [git-flow](https://datasift.github.io/gitflow/IntroducingGitFlow.html).

For our purposes, the general concept behind gitflow has been extended to allow
for these additional needs:

1. We have two different types of releases, mainnet and testnet, with a master-like branch for each one.
2. We have to distinguish consensus-breaking changes (aka hardforks) from
   non-consensus-breaking changes.

## Goals To Achieve:

1. Maintain two independent release versions, testnet and mainnet.
2. Decouple development from releases, i. e. maintain the ability to create
   emergency bugfixes for current release without bringing incomplete new
   features into production.
3. Separate consensus-related changes from non-consensus-related changes.
4. Keep development branches compatible with mainnet.

## Basic Rules:

1. Development always happens in private feature-branches. The only exception is
   a change that must be distinguished in the destination branch (typical
   example: hardfork date in testnet).
2. Features are merged after they are reasonably complete, i. e. they come with
   unit tests that provide reasonable coverage and do not report any errors.
    1. "Completed" features that are not consensus-related are merged into
       "develop".
    2. "Completed" features that are consensus-related are merged into the
       "hardfork" branch, with a hardfork date in the far future.
    3. All merges into "develop" or "hardfork" are performed via github PR's and
       require review and approval from core developers (if the PR is created
       by a core dev at least one other core dev must review and approve).
    4. To maintain a clean history and make reviewing and merging easier,
       feature branches must be rebased onto current "develop" (or "hardfork")
       before creating a PR.
    5. Merges are always done as real merges, not as fast-forwards, and not
       squashed.
    6. For each branch, maintain a correct `sonar.branch.target` value in the
       `sonar-project.properties` file until we develop a script to maintain it.
3. Core developers coordinate regular merges from "develop" into "hardfork".
4. Both "develop" and "hardfork" should always remain compatible with mainnet,
   i. e. a full replay must be possible.
   * the planned version is set in the `sonar-project.properties` file


## How To Create a Release

For a release,

0. Check and bump stuff
   1. Check whether need to bump `DB_VERSION` to force a replay after upgraded:
   if there is a data schema change, or logic change that affects historical
   data, the answer is yes.
   2. FC version usually has been bumped already during development, but it
   doesn't harm if check again.
   3. Check version in the `sonar-project.properties` file
   4. May need to bump baseimage version in Dockerfile
   5. Check reflection with `doxygen && programs/build_helpers/check_reflect.py`
1. A "release" branch is created based on "develop" or "hardfork".
2. A draft of release notes is prepared as a Github issue, with links to issues
   and pull requests, and info that users might have interest in.
3. The "release" branch is merged into "testnet".
4. For a hardfork release, the hardfork date is adapted directly on the
   testnet branch.
5. The "testnet" branch is tagged as test-<i>version</i>. A pre-release is
   created for that tag, with a link to the draft release notes.
   * Create binaries for Linux, macOS and Windows with
     [bitshares-gitian](https://github.com/bitshares/bitshares-gitian) and
     upload them to the pre-release page.
     * Note 1: some license files are stored in bitshares-gitian. If any of
       them changed, need to update the copy in bitshares-gitian.
     * Note 2: we may like to update libraries in bitshares-gitian, E.G. OpenSSL,
       zlib and Curl may have new releases.
     * Note 3: create a new branch in bitshares-gitian for each new release or
       pre-release of bitshares-core.
   * A Docker image should be built and pushed to Docker Hub automatically
     by Github Actions. Check it.
     Link: https://hub.docker.com/r/bitshares/bitshares-core
6. Bugfixes for the release are created on the "release" branch and merged into
   "testnet". Additional test-<i>version</i>s are tagged as needed.
   Additional pre-releases too.
7. After sufficient testing, the release must be approved. In the case of a
   hardfork release, witness approval is required.
8. After approval, a *version* will be decided. If it is a hardfork release,
   the mainnet hardfork date is decided and set in the "release" branch.
9. Bump docs sub-module which links to wiki in the "release" branch.
10. Update Doxyfile with the planned version in the "release" branch.
11. Update CONTRIBUTORS.txt in the "release" branch according to instructions
    inside. Update the CONTRIBUTORS.txt file in FC too, and bump FC if needed.
12. The "release" branch is merged into "master", and a *version* tag is created
    on "master". Create the tag by git command line and push to github.
    This is needed for Docker to catch the new tag.
    * Note 1: normally, a docker build will be triggered automatically in Github
      Actions after the tag is pushed, and a new Docker image will be pushed to
      Docker Hub when it's done.
    * Note 2: The project description on Docker Hub is not updated automatically.
      Edit it manually (copy and modify from README.md) if needed.
13. Create binaries for Linux, macOS and Windows with
    [bitshares-gitian](https://github.com/bitshares/bitshares-gitian).
    * Note 1: some license files are stored in bitshares-gitian. If any of
      them changed, need to update the copy in bitshares-gitian.
    * Note 2: we may like to update libraries in bitshares-gitian, E.G. OpenSSL,
      zlib and Curl may have new releases.
    * Note 3: create a new branch in bitshares-gitian for each new release or
      pre-release of bitshares-core.
14. Create the release on Github by attaching the prepared release notes to the
    tag, attach the binaries and add checksums of the binaries in the release
    notes.
15. Update online code documentation by using updated `Doxyfile` as config file
    in the `master` branch.
    * Send pull request to https://github.com/bitshares/doxygen.bitshares.org with
      new content in html format.
      * Note: the HTML files are hosted on Github Pages, thus the `CNAME` file in
        the target directory need to be kept. 
      * Note: need to edit `index.html`, remove the "Build Status" CI badges.
    * Send pull request to https://github.com/bitshares/dev.bitshares.works with
      new content in xml format.
16. <del>Update date of last core [download page of bitshares.org site](
    https://github.com/bitshares/bitshares.org/blob/master/download.php).</del>
17. The "release" branch is merged back into "develop" and "hardfork".
18. The "release" branch is merged into "testnet". This will produce a merge
    conflict for the hardfork dates, which must be resolved without changing the
    testnet hardfork date.

**Note:** Solving conflicts by github(web) will merge branches in unintended
directions. Avoid solving this way, merge and resolve conflicts manually through
the git command line. Conflicts generally occur when merging release to testnet.

**Note 2:** Follow command line github suggestion to resolve conflicts but at the
end of the process you will not have permission to merge directly to `testnet`,
never push the fix to `release`. Create a new branch and push there, then create
a new pull request between `testnet` and `new_branch`, merge `new_branch` to
`testnet` and `release` will be automatically added to the merge.

**Note 3:** When creating tag for testnet do it from the command line with
`git tag`. Github don't have the option to create a tag without a release.

**Note 4:** Don't change tags on github. This is a source of confusion, and of
irreproducible bug reports. Making a new one is better (ex: test-3.0.1).

**Note 5:** Do not mark mainnet releases as "pre-release" unless there is a new
version coming immediately after.
Never upgrade "pre-release" to "release" as new emails to subscribers will not
be sent when doing so.


## How To Create an Emergency Fix

An emergency fix may become necessary when a serious problem in mainnet is
discovered. The goal here is to fix the problem as soon as possible, while
keeping the risk for creating additional problems as low as possible.

First of all, the problem must be analyzed and debugged. This happens,
naturally, directly on the release version.

Presumably the developer who creates the fix will work on his private master
branch. That is OK. But for publishing the fix, the following steps should be
taken:

1. Find the version of the "release" branch that was merged into "master"
   when creating the broken release version, name it "old-release".
2. The fix is applied to "old-release" via pull requests.
3. The new "old-release" branch (with the fix) is merged into "master", and
   a *version* tag is created on "master".
   * A new Docker image will be built and pushed to Docker Hub automatically.
4. Witnesses update to the new *version*, and production continues.
5. A unit test is created on the original "old-release" (without the fix)
   that reproduces the problem.
6. The two "old-release" branches (one with the fix and the other with the
   unit test) are merged, and it is verified that the fix resolves the
   problem, by running the unit test.
7. Follow the normal release workflow to create a release, upload binaries,
   update docs and etc.
8. The new "old-release" branch (with both the fix and the unit test) is
   merged into "testnet". Follow the normal workflow to create a testnet
   release.
9. The new "old-release" branch (with both the fix and the unit test) is
   merged back into "develop", and if needed, into "hardfork" and the
   current "release".

## How To Update the README.md

In general we want changes in the README to be visible fast in `master`.

1. If the `release` branch contains changes that we don't want to merge into
   `master` yet, create a new branch from the version of the latest `release`
   branch that was merged into `master`, and update the README.md file in the
   new branch via pull requests. Otherwise, update the README.md file in the
   `release` branch via pull requests.
2. If created a new branch in the last step, merge the new branch into `release`
   and `master`. Otherwise, merge `release` into `master`.
3. Merge `release` back into `develop`.
4. Merge `release` into `testnet` and/or merge `develop` into `hardfork` if
   changes need to be visible in the branches, or wait until these merges
   occur naturally.