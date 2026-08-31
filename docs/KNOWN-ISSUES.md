# Known issues

Things that are understood, worked around, and worth doing properly.

## Ant Design is handed CSS variables it cannot read

**Where:** `main/http_server/axe-os/src/pages/App.vue`, the `a-config-provider`
theme block.

The theme is built on CSS custom properties, and the Ant Design tokens are set
to those properties as strings:

```js
colorPrimary: 'var(--ant-primary-color)',
colorInfo:    'var(--ant-primary-color)',
colorBgBase:  'var(--surface-ground)',
```

Ant Design v4 computes its palettes in JavaScript — it derives hover states,
active states, borders and contrasting text from `colorPrimary` before any CSS
is involved. A custom property is opaque to that, so every derived colour comes
out unusable. What reaches the page is whatever its fallback produces, which is
usually close to the surrounding background.

This is not a styling preference. It is a whole class of defects, and it has
produced at least three:

- the selected card tab rendered as an empty box, because Ant Design paints the
  label on an inner `.ant-tabs-tab-btn` and gave it a colour close to the tab's
  own background — the text was present and invisible
- an enabled switch looked identical to a disabled one, so there was no way to
  see whether dual mining was on
- anything else derived from the accent colour is suspect until checked

**Current state:** the three known cases are corrected in
`main/http_server/axe-os/src/styles/layout/_antd-fixes.scss`, which sets the
affected properties in ordinary CSS where the variables resolve correctly. That
file is a patch over the cause, not a fix for it, and it will grow every time
another component turns out to derive a colour the same way.

**The real fix** is to stop passing custom properties into the token block and
give Ant Design real colour values, driven from the same source as the CSS
variables so the two cannot drift:

- keep the palette in one place as literal values, in TypeScript
- derive the CSS custom properties from it, and pass the same literals to
  `a-config-provider`
- re-provide the tokens when the theme changes, so switching theme updates both
  halves together

The theme selector offers several accents, so whatever replaces this has to
survive a theme change at runtime rather than being read once at start-up.

**Why it was not done at the time:** it lands in the middle of the component
that mounts the whole interface, on a day that had already produced three
firmware releases. Rewriting the theme system while shipping is how something
breaks quietly. It wants a clear run and a careful look at every component that
reads an accent colour.

## The global stylesheet is compiled twice

**Where:** `main/http_server/axe-os/src/pages/App.vue`, line 305.

`main.ts` imports `styles/layout/layout.scss` globally. `App.vue` then imports
the same file again inside a `<style scoped>` block, so every rule in it is
emitted a second time carrying that component's scope attribute.

Two consequences. The CSS payload is roughly double what it needs to be. And
the scoped copies cannot reach anything Ant Design renders in a portal — a
dropdown, a modal — which is the same trap that left the pool logos unstyled
for as long as they were.

The fix is to delete the `@import` from `App.vue`; the global import in
`main.ts` already covers it. It is one line, but it is in the component that
mounts the entire interface, so it wants a build and a look at the running UI
rather than being done in passing.

Found while checking that the corrections in `_antd-fixes.scss` had reached the
device: the same rules appeared twice in the built bundle, once with a scope
attribute and once without.
