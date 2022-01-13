function Link(el)
  el.target = string.gsub(el.target, "TESTED.md", "tested.html")
  el.target = string.gsub(el.target, "CHANGELOG.md", "changelog.html")
  return el
end
