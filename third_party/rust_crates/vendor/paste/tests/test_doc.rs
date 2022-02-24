use paste::paste;

#[test]
fn test_paste_doc() {
    macro_rules! m {
        ($ret:ident) => {
            paste! {
                #[doc = "Create a new [`" $ret "`] object."]
                fn new() -> $ret { todo!() }
            }
        };
    }

    struct Paste;
    m!(Paste);

    let _ = new;
}

macro_rules! get_doc {
    (#[doc = $literal:tt]) => {
        $literal
    };
}

#[test]
fn test_escaping() {
    let doc = paste! {
        get_doc!(#[doc = "s\"" r#"r#""#])
    };

    let expected = "s\"r#\"";
    assert_eq!(doc, expected);
}

#[test]
fn test_literals() {
    let doc = paste! {
        get_doc!(#[doc = "int=" 0x1 " bool=" true " float=" 0.01])
    };

    let expected = "int=0x1 bool=true float=0.01";
    assert_eq!(doc, expected);
}