use nom::error::ParseError;
use std::num::NonZeroUsize;

pub fn decode_jaguar<'a, E: ParseError<&'a [u8]>>(
    input: &'a [u8],
    cap: usize,
) -> nom::IResult<&'a [u8], Vec<u8>, E> {
    let mut get_id_byte = 0u8;
    let mut id_byte = 0;
    let mut iter = input.iter().copied();
    let mut count = 0;
    let mut next = || {
        let b = iter.next().ok_or_else(|| {
            nom::Err::Error(nom::error::make_error(
                &input[count..],
                nom::error::ErrorKind::Eof,
            ))
        })?;
        count += 1;
        Ok(b)
    };
    let mut output = Vec::with_capacity(cap);

    loop {
        if get_id_byte == 0 {
            id_byte = next()?;
        }
        get_id_byte = (get_id_byte + 1) & 7;
        if id_byte & 1 != 0 {
            const LENSHIFT: u32 = 4;
            let pos = (next()? as i32) << LENSHIFT;
            let d = next()? as i32;
            let pos = pos | (d >> LENSHIFT);
            let len = (d & 0xf) + 1;
            if len == 1 {
                break;
            }
            if len > 0 {
                let mut i = 0;
                let source = output.len() - pos as usize - 1;
                if len & 3 != 0 {
                    while i != len & 3 {
                        output.push(output[source + i as usize]);
                        i += 1;
                    }
                }
                while i != len {
                    for _ in 0..4 {
                        output.push(output[source + i as usize]);
                        i += 1;
                    }
                }
            }
        } else {
            output.push(next()?);
        }
        id_byte >>= 1;
    }
    Ok((&[], output))
}

#[repr(transparent)]
#[derive(Copy, Clone)]
struct NodeIndex(NonZeroUsize);

impl NodeIndex {
    #[inline]
    fn new(val: usize) -> Self {
        Self(NonZeroUsize::new(val.checked_add(1).unwrap()).unwrap())
    }
    #[inline]
    fn get(&self) -> usize {
        self.0.get() - 1
    }
}

#[derive(Default, Copy, Clone)]
#[repr(align(16))]
struct LzssNode<'a> {
    pointer: Option<&'a [u8]>,
    prev: Option<NodeIndex>,
    next: Option<NodeIndex>,
}
#[derive(Default, Copy, Clone)]
struct LzssList {
    start: Option<NodeIndex>,
    end: Option<NodeIndex>,
}
struct LzssEncoder<'a> {
    hashtable: [LzssList; 256],
    hashtarget: [LzssNode<'a>; 4096],
}

impl<'a> Default for LzssEncoder<'a> {
    #[inline]
    fn default() -> Self {
        Self {
            hashtable: [Default::default(); 256],
            hashtarget: [Default::default(); 4096],
        }
    }
}

impl<'a> LzssEncoder<'a> {
    fn add_node(&mut self, pointer: &'a [u8]) {
        let targetindex = (pointer.as_ptr() as usize) % self.hashtarget.len();
        let target = &self.hashtarget[targetindex];
        if let Some(tpointer) = target.pointer {
            let list = &mut self.hashtable[tpointer[0] as usize];
            if let Some(prev) = target.prev {
                list.end = Some(prev);
                self.hashtarget[prev.get()].next = None;
            } else {
                list.end = None;
                list.start = None;
            }
        }
        let target = &mut self.hashtarget[targetindex];
        let list = &mut self.hashtable[pointer[0] as usize];
        target.pointer = Some(pointer);
        target.prev = None;
        target.next = list.start;
        let target = Some(NodeIndex::new(targetindex));
        if let Some(start) = list.start {
            self.hashtarget[start.get()].prev = target;
        } else {
            list.end = target;
        }
        list.start = target;
    }
}

pub fn encode_jaguar(input: &[u8]) -> Vec<u8> {
    let mut output = Vec::with_capacity(input.len() * 9 / 8 + 1);
    let mut encoder = LzssEncoder::default();
    let mut inputlen = input.len();
    let mut encodedpos = 0usize;
    let mut lookahead = 0usize;
    let mut putidbyte = 0u8;
    let mut idbytepos = 0usize;

    while inputlen > 0 {
        if putidbyte == 0 {
            idbytepos = output.len();
            output.push(0);
        }
        putidbyte = (putidbyte + 1) & 7;
        let mut encodedlen = 0;
        let lookaheadlen = inputlen.min(16);
        let mut hashp = encoder.hashtable[input[lookahead] as usize].start;
        while let Some(hp) = hashp {
            let hp = encoder.hashtarget[hp.get()];
            let mut samelen = 0;
            let mut len = lookaheadlen;
            while len > 0 && hp.pointer.unwrap()[samelen] == input[lookahead + samelen] {
                samelen += 1;
                len -= 1;
            }
            if samelen > encodedlen {
                encodedlen = samelen;
                encodedpos = hp.pointer.unwrap().as_ptr() as usize - input.as_ptr() as usize;
            }
            if samelen == lookaheadlen {
                break;
            }
            hashp = hp.next;
        }
        if encodedlen >= 3 {
            output[idbytepos] = (output[idbytepos] >> 1) | 0x80;
            output.push(((lookahead - encodedpos - 1) >> 4) as u8);
            output.push((((lookahead - encodedpos - 1) << 4) | (encodedlen - 1)) as u8);
        } else {
            encodedlen = 1;
            output[idbytepos] >>= 1;
            output.push(input[lookahead]);
        }
        for _ in 0..encodedlen {
            encoder.add_node(&input[lookahead..]);
            lookahead += 1;
        }
        inputlen -= encodedlen;
    }
    if putidbyte == 0 {
        output.push(1);
    } else {
        output[idbytepos] = ((output[idbytepos] >> 1) | 0x80) >> (7 - putidbyte);
    }
    output.push(0);
    output.push(0);
    output
}
