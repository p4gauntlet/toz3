The pruner's exact output tests require the P4C ToP4 spacing fix from
`fruffy/p4fmt-01-top4-fix`, commit `6dc763840e91bb00a8f649d0bea847cb9b86ea60`.
The accompanying patch contains only the printer change and can be applied from
the P4C source directory with:

```sh
git apply extensions/toz3/patches/p4c-top4-system-declarations.patch
```

The fix skips system-only error and match-kind namespaces before emitting a
separator. It retains user extensions and output with includes disabled. The
branch's three regression tests pass with the patch; the spacing regression
fails without it. All four pruner golden comparisons pass with the fix, without
changing the references or ignoring whitespace.
