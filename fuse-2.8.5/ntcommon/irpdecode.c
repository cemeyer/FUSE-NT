#ifdef __CYGWIN__

// Decodes an IRP (and associated IO stack) to locate the current stack entry
// and the IRP major number.
//
// Returns non-negative on success.
int fusent_decode_irp(IRP *irp, IO_STACK_LOCATION *iosp, uint8_t *outirptype,
		IO_STACK_LOCATION **outiosp)
{
	// CurrentLocation is 1-indexed:
	*outiosp = &iosp[irp->CurrentLocation - 1];
	*outirptype = (*outiosp)->MajorFunction;

	return 0;
}

#endif /* __CYGWIN__ */
