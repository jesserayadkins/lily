enum class Option[A] {
	Some(A),
	None
}

var a: any = Some(10)

if a != Some(10):
	print("Failed!\n")
