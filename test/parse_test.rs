use std::{
    error::Error,
    ffi::OsStr,
    fs, io,
    path::{Path, PathBuf},
};
use sxpb::{Value, from_str, to_string};

fn collect_files(directory: &Path, files: &mut Vec<PathBuf>) -> io::Result<()> {
    for entry in fs::read_dir(directory)? {
        let path = entry?.path();
        let metadata = path.metadata()?;
        if metadata.is_dir() {
            collect_files(&path, files)?;
        } else if metadata.is_file() && path.extension() == Some(OsStr::new("sxpb")) {
            files.push(path);
        }
    }
    Ok(())
}

fn without_terminal_lf(text: &str) -> &str {
    text.strip_suffix('\n').unwrap_or(text)
}

#[test]
fn shared_examples_preserve_precise_values_and_canonical_fixtures() -> Result<(), Box<dyn Error>> {
    let root = Path::new(env!("CARGO_MANIFEST_DIR"));
    let directories = from_str(&fs::read_to_string(root.join("test/dirs.sxpb"))?)?;
    let directories = directories
        .as_list()
        .ok_or("test/dirs.sxpb must contain a list")?;
    let mut files = Vec::new();
    for directory in directories {
        let Value::String(directory) = directory else {
            return Err("test/dirs.sxpb entries must be strings".into());
        };
        collect_files(&root.join(directory), &mut files)?;
    }
    files.sort();
    files.dedup();
    assert!(!files.is_empty(), "No .sxpb files found");
    let mut golden_count = 0;
    let mut idempotent_count = 0;
    for path in &files {
        let group = path.parent().and_then(Path::file_name);
        let canonical = group == Some(OsStr::new("idempotent"));
        golden_count += usize::from(group == Some(OsStr::new("golden")));
        idempotent_count += usize::from(canonical);
        let source = fs::read_to_string(path)?;
        let value = from_str(&source)?;
        let printed = to_string(&value)?;
        if canonical {
            assert_eq!(
                without_terminal_lf(&source),
                without_terminal_lf(&printed),
                "Canonical print changed {}",
                path.display()
            );
        }
        let reparsed = from_str(&printed)?;
        assert_eq!(
            value,
            reparsed,
            "Precise roundtrip changed {}",
            path.display()
        );
        assert_eq!(
            printed,
            to_string(&reparsed)?,
            "Printer is not idempotent for {}",
            path.display()
        );
        println!("Validated {}", path.display());
    }
    assert!(golden_count > 0, "No golden corpus files found");
    assert!(idempotent_count > 0, "No idempotent corpus files found");
    println!(
        "{} examples, including {golden_count} golden and {idempotent_count} idempotent",
        files.len()
    );
    Ok(())
}
