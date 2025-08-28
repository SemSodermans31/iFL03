// Node script to sync SVG paths from current-tracks.coffee into track-gen.html
// Usage: node assets/tracks/generation/sync-paths.js

const fs = require('fs');
const path = require('path');

const ROOT = process.cwd();
const coffeePath = path.join(ROOT, 'assets', 'tracks', 'generation', 'current-tracks.coffee');
const htmlPath = path.join(ROOT, 'assets', 'tracks', 'generation', 'track-gen.html');

function read(file) {
  return fs.readFileSync(file, 'utf8');
}

// Extract {key, trackId, svgPath} from current-tracks.coffee
function parseCoffee(coffeeText) {
  const lines = coffeeText.split(/\r?\n/);
  const results = [];
  let currentKey = null;
  let currentTrackId = null;
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    // key line: \t\tname:
    const keyMatch = line.match(/^\t\t([a-zA-Z0-9_]+):\s*$/);
    if (keyMatch) {
      currentKey = keyMatch[1];
      currentTrackId = null;
      continue;
    }
    if (!currentKey) continue;
    const idMatch = line.match(/^\t\t\ttrackId:\s*(\d+)\s*$/);
    if (idMatch) {
      currentTrackId = parseInt(idMatch[1], 10);
      continue;
    }
    const pathStartMatch = line.match(/^\t\t\tpaths:\s*\[\s*'(.*)'\s*\]\s*(?:#.*)?$/);
    if (pathStartMatch) {
      const svg = pathStartMatch[1];
      results.push({ key: currentKey, trackId: currentTrackId, svgPath: svg });
      currentKey = null;
      currentTrackId = null;
    }
  }
  return results;
}

// Build a normalization map to convert Coffee keys to HTML keys
function normalizeKey(key) {
  // common replacements
  let k = key
    .replace(/_oval$/,'_oval')
    .replace(/_road$/,'_road')
    .replace(/_gp$/,'_gp')
    .replace(/_grandprix$/,'_grandprix')
    .replace(/_legend(s)?_oval$/,'_legendsoval')
    .replace(/auto_club_/,'california_')
    .replace(/^atlanta_road$/,'atlanta_roadcourse')
  ;
  return k;
}

// Parse track-gen.html and return full text plus indices of path arrays
function parseHtml(htmlText) {
  // Match entries like: key: {\n  trackId: N,\n  paths: ['...']
  const entryRegex = /(\t\t([a-zA-Z0-9_]+):\s*\{[\s\S]*?\t\t\tpaths:\s*\[\s*')(.*?)('\s*\][\s\S]*?\})/g;
  const entries = [];
  let m;
  while ((m = entryRegex.exec(htmlText)) !== null) {
    const full = m[0];
    const leading = m[1];
    const key = m[2];
    const currentPath = m[3];
    const trailingQuote = m[4];
    const start = m.index;
    const end = m.index + full.length;
    entries.push({ key, currentPath, start, end, leading, trailingQuote });
  }
  return { htmlText, entries };
}

function main() {
  const coffeeText = read(coffeePath);
  const htmlText = read(htmlPath);

  const coffeeEntries = parseCoffee(coffeeText);
  const coffeeByKey = new Map();
  for (const e of coffeeEntries) {
    coffeeByKey.set(e.key, e);
    const norm = normalizeKey(e.key);
    if (norm !== e.key) coffeeByKey.set(norm, e);
  }

  const { entries } = parseHtml(htmlText);
  let updated = 0;
  let skipped = 0;
  let unchanged = 0;
  let newHtml = htmlText;

  // We'll build replacements from back to front to preserve indices
  const replacements = [];
  for (const ent of entries) {
    const coffee = coffeeByKey.get(ent.key);
    if (!coffee) { skipped++; continue; }
    if (coffee.svgPath && coffee.svgPath.length > 0) {
      if (ent.currentPath === '' || ent.currentPath === "" || ent.currentPath.trim() === '') {
        // replace within this entry the paths content
        const search = new RegExp('(\t\t\tpaths:\\s*\\[\\s*\')' + ent.currentPath.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '(\'\\s*\\])');
        // Safer direct segment reconstruction using captured groups from parse regex
        const segment = newHtml.slice(ent.start, ent.end);
        const replacedSegment = segment.replace(/(\t\t\tpaths:\s*\[\s*')(.*?)('\s*\])/, `$1${coffee.svgPath}$3`);
        replacements.push({ start: ent.start, end: ent.end, text: replacedSegment });
        updated++;
      } else {
        unchanged++;
      }
    } else {
      skipped++;
    }
  }

  // Apply replacements from the end to avoid shifting indices
  replacements.sort((a,b) => b.start - a.start);
  for (const r of replacements) {
    newHtml = newHtml.slice(0, r.start) + r.text + newHtml.slice(r.end);
  }

  if (updated > 0) {
    fs.writeFileSync(htmlPath, newHtml, 'utf8');
  }

  console.log(JSON.stringify({ totalHtmlEntries: entries.length, coffeeEntries: coffeeEntries.length, updated, unchanged, skipped }, null, 2));
}

main();


