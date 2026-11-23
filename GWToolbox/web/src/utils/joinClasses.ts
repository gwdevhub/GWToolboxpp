// Joins the class names that are set, leaving out the ones that aren't.

export const joinClasses = (...names: (string | false | undefined | null)[]) => names.filter(Boolean).join(' ');
